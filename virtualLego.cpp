////////////////////////////////////////////////////////////////////////////////
//
// File: virtualLego.cpp
//
// Original Author: 박창현 Chang-hyeon Park, 
// Modified by Bong-Soo Sohn and Dong-Jun Kim
// 
// Originally programmed for Virtual LEGO. 
// Modified later to program for Virtual Billiard.
//        
////////////////////////////////////////////////////////////////////////////////

#include "d3dUtility.h"
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

IDirect3DDevice9* Device = NULL;

// window size
const int Width = 1920;
const int Height = 1080;
double TANK_SPEED = 0.45;

// -----------------------------------------------------------------------------
// Transform matrices
// -----------------------------------------------------------------------------
D3DXMATRIX g_mWorld;
D3DXMATRIX g_mView;
D3DXMATRIX g_mProj;

#define M_RADIUS 0.06   // ball radius
#define PI 3.14159265
#define M_HEIGHT 0.01
#define DECREASE_RATE 0.9982
#define TANK_VELOCITY_RATE 0.99

#define BLUEBALL_VELOCITY 0.8 // blueball 조작 속도
#define MAX_BLUEBALL_RADIUS 15  // blueball 어디까지 멀어질 수 있는지
#define MIN_BLUEBALL_RADIUS 0.4 // blueball 어디 이상 멀어져야 하는지 (앞으로)
#define MAX_BLUEBALL_WIDTH 1 // blueball 어디까지 멀어질 수 있는지 (옆으로)

#define MISSILE_POWER 1.25
#define MISSILE_GRAVITY_RATE 3.5
#define MISSILE_DECREASE_RATE 0.9975  // 미사일 마찰력
#define MISSILE_EXPOLSION_RADIUS M_RADIUS+1.5 // 미사일 폭발 반경

#define WORLD_WIDTH 24
#define WORLD_DEPTH 100
#define BORDER_WIDTH 0.12f // 가장자리 벽 굵기

#define NUM_OBSTACLE 20
#define TANK_DISTANCE 1000


bool GAME_START = false;
bool GAME_FINISH = false;
float MOVEMENT = 0.0f;
float Tank_spawn_point[3] = { 0, 0.2f, -WORLD_DEPTH / 2 + 5 };
int camera_option = 0;
// -----------------------------------------------------------------------------
// CSphere class definition
// -----------------------------------------------------------------------------

class CSphere {
	//private
protected:
	float					center_x, center_y, center_z;
	float                   m_radius;
	float					m_velocity_x;
	float					m_velocity_y;
	float					m_velocity_z;
	bool					created;  // 월드에 존재하는지

public:
	CSphere(void)
	{
		D3DXMatrixIdentity(&m_mLocal);
		ZeroMemory(&m_mtrl, sizeof(m_mtrl));
		m_radius = 0;
		m_velocity_x = 0;
		m_velocity_y = 0;
		m_velocity_z = 0;

		m_pSphereMesh = NULL;
	}
	~CSphere(void) {}

public:
	bool create(IDirect3DDevice9* pDevice, D3DXCOLOR color = d3d::WHITE)
	{
		if (NULL == pDevice)
			return false;

		m_mtrl.Ambient = color;
		m_mtrl.Diffuse = color;
		m_mtrl.Specular = color;
		m_mtrl.Emissive = d3d::BLACK;
		m_mtrl.Power = 5.0f;

		created = true;

		if (FAILED(D3DXCreateSphere(pDevice, getRadius(), 50, 50, &m_pSphereMesh, NULL)))
			return false;
		return true;
	}

	bool getCreated() { return created; }

	void destroy(void)
	{
		created = false;
		if (m_pSphereMesh != NULL) {
			m_pSphereMesh->Release();
			m_pSphereMesh = NULL;
		}
	}

	void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
	{
		if (NULL == pDevice)
			return;
		if (!created) return;
		pDevice->SetTransform(D3DTS_WORLD, &mWorld);
		pDevice->MultiplyTransform(D3DTS_WORLD, &m_mLocal);
		pDevice->SetMaterial(&m_mtrl);
		m_pSphereMesh->DrawSubset(0);
	}


	bool hasIntersected(CSphere& ball)
	{
		D3DXVECTOR3 sphereCenter = ball.getCenter();
		float sphereRadius = ball.getRadius();
		D3DXVECTOR3 ballCenter = getCenter();
		float ballRadius = getRadius();
		// Check for intersection along each axis (X, Y, Z)
		bool intersectX = (sphereCenter.x + sphereRadius >= ballCenter.x - ballRadius) &&
			(sphereCenter.x - sphereRadius <= ballCenter.x + ballRadius);
		bool intersectY = (sphereCenter.y + sphereRadius >= ballCenter.y - ballRadius) &&
			(sphereCenter.y - sphereRadius <= ballCenter.y + ballRadius);
		bool intersectZ = (sphereCenter.z + sphereRadius >= ballCenter.z - ballRadius) &&
			(sphereCenter.z - sphereRadius <= ballCenter.z + ballRadius);
		return intersectX && intersectY && intersectZ;
	}

	void hitBy(CSphere& ball)	// 공끼리 충돌하면 공이 전부 사라짐
	{
		if (hasIntersected(ball)) {
			destroy();
			ball.destroy();
		}
	}

	void hitBy() {
		if (center_y <= M_RADIUS) {
			destroy();
		}
	}

	void Out()
	{
		if (center_x >= 4.5 - M_RADIUS || center_x <= -4.5 + M_RADIUS || center_z >= 3 - M_RADIUS || center_z <= -3 + M_RADIUS)
			if (center_y <= M_RADIUS)
				destroy();
	}

	void ballUpdate(float timeDiff)
	{
		if (!created) return;
		const float TIME_SCALE = 3.3;
		D3DXVECTOR3 cord = this->getCenter();
		double vx = abs(this->getVelocity_X());
		double vy = abs(this->getVelocity_Y());
		double vz = abs(this->getVelocity_Z());

		float tX = cord.x + TIME_SCALE * timeDiff * m_velocity_x;
		float tY = cord.y + TIME_SCALE * timeDiff * m_velocity_y;
		float tZ = cord.z + TIME_SCALE * timeDiff * m_velocity_z;

		if (tY < 0 + M_RADIUS)
			tY = M_RADIUS;
		// y가 0 이하로 떨어지지 않도록 (임시)

		this->setCenter(tX, tY, tZ);
		Out();	// 미사일이 밖으로 나가면 바닥에 닿을 때 터짐

		//this->setPower(this->getVelocity_X() * DECREASE_RATE, this->getVelocity_Z() * DECREASE_RATE);

		double rate = 1 - (1 - MISSILE_DECREASE_RATE) * timeDiff * 400;
		if (rate < 0)
			rate = 0;
		this->setPower(getVelocity_X() * rate, getVelocity_Y() - MISSILE_GRAVITY_RATE * timeDiff, getVelocity_Z() * rate);

	}

	double getVelocity_X() { return this->m_velocity_x; }
	double getVelocity_Y() { return this->m_velocity_y; }
	double getVelocity_Z() { return this->m_velocity_z; }


	void setPower(double vx, double vz)
	{
		this->m_velocity_x = vx;
		this->m_velocity_z = vz;

	}

	void setPower(double vx, double vy, double vz)
	{
		this->m_velocity_x = vx;
		this->m_velocity_y = vy;
		this->m_velocity_z = vz;
	}

	void setCenter(float x, float y, float z)
	{
		D3DXMATRIX m;
		center_x = x;	center_y = y;	center_z = z;
		D3DXMatrixTranslation(&m, x, y, z);
		setLocalTransform(m);
	}

	float getRadius(void)  const { return (float)(M_RADIUS); }
	const D3DXMATRIX& getLocalTransform(void) const { return m_mLocal; }
	void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }
	D3DXVECTOR3 getCenter(void) const
	{
		D3DXVECTOR3 org(center_x, center_y, center_z);
		return org;
	}

	bool get_created() {
		return created;
	}

private:
	D3DXMATRIX              m_mLocal;
	D3DMATERIAL9            m_mtrl;
	ID3DXMesh* m_pSphereMesh;

};

// -----------------------------------------------------------------------------
// CWall class definition
// -----------------------------------------------------------------------------

class CWall {

private:

	float					m_x;
	float					m_y;
	float					m_z;
	float                   m_width;
	float                   m_depth;
	float					m_height;
	bool					created;

public:
	CWall(void)
	{
		D3DXMatrixIdentity(&m_mLocal);
		ZeroMemory(&m_mtrl, sizeof(m_mtrl));
		m_width = 0;
		m_depth = 0;
		m_pBoundMesh = NULL;
	}
	~CWall(void) {}
public:
	bool create(IDirect3DDevice9* pDevice, float ix, float iz, float iwidth, float iheight, float idepth, D3DXCOLOR color = d3d::WHITE)
	{
		if (NULL == pDevice)
			return false;

		m_mtrl.Ambient = color;
		m_mtrl.Diffuse = color;
		m_mtrl.Specular = color;
		m_mtrl.Emissive = d3d::BLACK;
		m_mtrl.Power = 5.0f;

		m_width = iwidth;
		m_height = iheight;
		m_depth = idepth;

		created = true;

		if (FAILED(D3DXCreateBox(pDevice, iwidth, iheight, idepth, &m_pBoundMesh, NULL)))
			return false;
		return true;
	}
	void destroy(void)
	{
		created = false;
		if (m_pBoundMesh != NULL) {
			m_pBoundMesh->Release();
			m_pBoundMesh = NULL;
		}
	}
	void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
	{
		if (NULL == pDevice)
			return;
		pDevice->SetTransform(D3DTS_WORLD, &mWorld);
		pDevice->MultiplyTransform(D3DTS_WORLD, &m_mLocal);
		pDevice->SetMaterial(&m_mtrl);
		m_pBoundMesh->DrawSubset(0);
	}

	bool hasIntersected(CSphere& ball)
	{
		D3DXVECTOR3 sphereCenter = ball.getCenter();
		float sphereRadius = ball.getRadius();

		D3DXVECTOR3 wallCenter = getCenter();
		float wallWidth = m_width;
		float wallHeight = m_height;
		float wallDepth = m_depth;

		bool intersectX = (sphereCenter.x <= wallCenter.x + wallWidth / 2 + M_RADIUS * 0.8) &&
			(sphereCenter.x >= wallCenter.x - wallWidth / 2 - M_RADIUS * 0.8);

		bool intersectY = (sphereCenter.y <= wallCenter.y + wallHeight / 2 + M_RADIUS * 0.8) &&
			(sphereCenter.y >= wallCenter.y - wallHeight / 2 - M_RADIUS * 0.8);

		bool intersectZ = (sphereCenter.z <= wallCenter.z + wallDepth / 2 + M_RADIUS * 0.8) &&
			(sphereCenter.z >= wallCenter.z - wallDepth / 2 - M_RADIUS * 0.8);

		return intersectX && intersectY && intersectZ;
	}

	bool hasIntersected(double objX, double objY, double objZ, double radius) {
		// (objX, objY, objZ)에 있는 반지름 = radius인 공과 충돌했는가?
		D3DXVECTOR3 wallCenter = getCenter();
		float wallWidth = m_width;
		float wallHeight = m_height;
		float wallDepth = m_depth;

		bool intersectX = (objX <= wallCenter.x + wallWidth / 2 + radius * 0.8) &&
			(objX >= wallCenter.x - wallWidth / 2 - radius * 0.8);

		bool intersectY = (objY <= wallCenter.y + wallHeight / 2 + radius * 0.8) &&
			(objY >= wallCenter.y - wallHeight / 2 - radius * 0.8);

		bool intersectZ = (objZ <= wallCenter.z + wallDepth / 2 + radius * 0.8) &&
			(objZ >= wallCenter.z - wallDepth / 2 - radius * 0.8);

		return intersectX && intersectY && intersectZ;
	}

	bool hasIntersected(CWall& wall)
	{
		D3DXVECTOR3 Center = getCenter();
		float width = m_width;
		float depth = m_depth;
		float height = m_height;

		D3DXVECTOR3 wallCenter = wall.getCenter();
		float wallWidth = wall.getWidth();
		float wallDepth = wall.getDepth();
		float wallHeight = wall.getHeight();

		bool intersectX = (wallCenter.x - wallWidth / 2 <= Center.x + width / 2) && (wallCenter.x + wallWidth / 2 >= Center.x - width / 2);
		bool intersectY = (wallCenter.y - wallHeight / 2 <= Center.y + height / 2) && (wallCenter.y + wallHeight / 2 >= Center.y - height / 2);
		bool intersectZ = (wallCenter.z - wallDepth / 2 <= Center.z + depth / 2) && (wallCenter.z + wallDepth / 2 >= Center.z - depth / 2);

		return intersectX && intersectY && intersectZ;
	}

	void hitBy(CSphere& ball)	// 벽이랑 공이랑 충돌하면 공은 사라짐
	{
		if (hasIntersected(ball))
			ball.destroy();
	}

	void setPosition(float x, float y, float z)
	{
		D3DXMATRIX m;
		this->m_x = x;
		this->m_y = y;
		this->m_z = z;

		D3DXMatrixTranslation(&m, x, y, z);
		setLocalTransform(m);
	}

	D3DXVECTOR3 getCenter(void) const
	{
		D3DXVECTOR3 org(m_x, m_y, m_z);
		return org;
	}

	void setRotation(float angle)
	{
		D3DXMatrixRotationY(&m_mLocal, angle);
	}

	bool get_created() { return created; }

	float getWidth(void) const { return m_width; };
	float getDepth(void) const { return m_depth; };
	float getHeight(void) const { return m_height; }


	//void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }

	//private :
protected:
	void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }

	D3DXMATRIX              m_mLocal;
	D3DMATERIAL9            m_mtrl;
	ID3DXMesh* m_pBoundMesh;
};

// -----------------------------------------------------------------------------
// CObstacle class definition
// -----------------------------------------------------------------------------

class CObstacle : public CWall {
public:
	void hitBy(CSphere& missile) {
		missile.destroy();
		destroy();

	}
};

// -----------------------------------------------------------------------------
// CLight class definition
// -----------------------------------------------------------------------------

class CLight {
public:
	CLight(void)
	{
		static DWORD i = 0;
		m_index = i++;
		D3DXMatrixIdentity(&m_mLocal);
		::ZeroMemory(&m_lit, sizeof(m_lit));
		m_pMesh = NULL;
		m_bound._center = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		m_bound._radius = 0.0f;
	}
	~CLight(void) {}
public:
	bool create(IDirect3DDevice9* pDevice, const D3DLIGHT9& lit, float radius = 0.1f)
	{
		if (NULL == pDevice)
			return false;
		if (FAILED(D3DXCreateSphere(pDevice, radius, 10, 10, &m_pMesh, NULL)))
			return false;

		m_bound._center = lit.Position;
		m_bound._radius = radius;

		m_lit.Type = lit.Type;
		m_lit.Diffuse = lit.Diffuse;
		m_lit.Specular = lit.Specular;
		m_lit.Ambient = lit.Ambient;
		m_lit.Position = lit.Position;
		m_lit.Direction = lit.Direction;
		m_lit.Range = lit.Range;
		m_lit.Falloff = lit.Falloff;
		m_lit.Attenuation0 = lit.Attenuation0;
		m_lit.Attenuation1 = lit.Attenuation1;
		m_lit.Attenuation2 = lit.Attenuation2;
		m_lit.Theta = lit.Theta;
		m_lit.Phi = lit.Phi;
		return true;
	}
	void destroy(void)
	{
		if (m_pMesh != NULL) {
			m_pMesh->Release();
			m_pMesh = NULL;
		}
	}
	bool setLight(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
	{
		if (NULL == pDevice)
			return false;

		D3DXVECTOR3 pos(m_bound._center);
		D3DXVec3TransformCoord(&pos, &pos, &m_mLocal);
		D3DXVec3TransformCoord(&pos, &pos, &mWorld);
		m_lit.Position = pos;

		pDevice->SetLight(m_index, &m_lit);
		pDevice->LightEnable(m_index, TRUE);
		return true;
	}

	void draw(IDirect3DDevice9* pDevice)
	{
		if (NULL == pDevice)
			return;
		D3DXMATRIX m;
		D3DXMatrixTranslation(&m, m_lit.Position.x, m_lit.Position.y, m_lit.Position.z);
		pDevice->SetTransform(D3DTS_WORLD, &m);
		pDevice->SetMaterial(&d3d::WHITE_MTRL);
		m_pMesh->DrawSubset(0);
	}

	D3DXVECTOR3 getPosition(void) const { return D3DXVECTOR3(m_lit.Position); }

private:
	DWORD               m_index;
	D3DXMATRIX          m_mLocal;
	D3DLIGHT9           m_lit;
	ID3DXMesh* m_pMesh;
	d3d::BoundingSphere m_bound;
	bool created;
};

// -----------------------------------------------------------------------------
// Tank class definition
// -----------------------------------------------------------------------------

class Tank {
protected:
	float					m_velocity_x;
	float					m_velocity_z;
	bool					isO;
	bool					isDistanceZero;
	CWall tank_part[7];
	bool created;
	float distance;
	D3DXVECTOR3 last_coord;
public:
	Tank(bool isOtank) {
		m_velocity_x = 0;
		m_velocity_z = 0;
		isO = isOtank;
		distance = TANK_DISTANCE;
		isDistanceZero = FALSE;
	}

	bool create(IDirect3DDevice9* pDevice, float ix, float iz, D3DXCOLOR color = d3d::WHITE) {
		if (!tank_part[0].create(pDevice, ix, iz, 0.7f, 0.375f, 1.5f, color)) {
			return false;
		}
		if (!tank_part[1].create(pDevice, ix, iz, 0.55f, 0.32f, 0.825f, color)) {
			return false;
		}
		if (!tank_part[2].create(pDevice, ix, iz, 0.12f, 0.12f, 1.4f, color)) {
			return false;
		}
		if (!tank_part[3].create(pDevice, ix, iz, 0.12f, 0.2, 1.4f, d3d::BLACK)) {
			return false;
		}
		if (!tank_part[4].create(pDevice, ix, iz, 0.12f, 0.2, 1.4f, d3d::BLACK)) {
			return false;
		}
		if (!tank_part[5].create(pDevice, ix, iz, 0.122f, 0.2, 1.35f, d3d::DARKSLATEGRAY)) {
			return false;
		}
		if (!tank_part[6].create(pDevice, ix, iz, 0.122f, 0.2, 1.35f, d3d::DARKSLATEGRAY)) {
			return false;
		}
		created = true;
		return true;
	}

	void setPosition(float x, float y, float z) {
		tank_part[0].setPosition(x, y, z);
		if (isO)
			tank_part[1].setPosition(x, y + 0.35f, z + 0.3f);
		else
			tank_part[1].setPosition(x, y + 0.35f, z - 0.3f);
		tank_part[2].setPosition(x, y + 0.35f, z);
		tank_part[3].setPosition(x - 0.24375, y - 0.28f, z);
		tank_part[4].setPosition(x + 0.24375, y - 0.28f, z);
		tank_part[5].setPosition(x - 0.24375, y - 0.24f, z);
		tank_part[6].setPosition(x + 0.24375, 0.24f, z);

	}

	void setIsDistanceZero(bool isDist) {
		isDistanceZero = isDist;
	}

	bool getIsDistanceZero() {
		return isDistanceZero;
	}

	bool get_created()
	{
		return created;
	}

	void destroy()
	{
		for (int i = 0; i < 3; i++)
			tank_part[i].destroy();
		created = false;
	}

	bool hasIntersected(CSphere& missile)
	{
		return tank_part[0].hasIntersected(missile) || tank_part[1].hasIntersected(missile) || tank_part[2].hasIntersected(missile) || tank_part[3].hasIntersected(missile)
			|| tank_part[4].hasIntersected(missile) || tank_part[5].hasIntersected(missile) || tank_part[6].hasIntersected(missile);
	}

	bool hasIntersected(CObstacle& obstacle)
	{
		return tank_part[0].hasIntersected(obstacle) || tank_part[1].hasIntersected(obstacle) || tank_part[2].hasIntersected(obstacle);
	}

	void hitBy(CSphere& missile)
	{
		if (hasIntersected(missile)) {
			missile.destroy();
			destroy();
		}
	}

	void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
	{
		tank_part[0].draw(pDevice, mWorld);
		tank_part[1].draw(pDevice, mWorld);
		tank_part[2].draw(pDevice, mWorld);
		tank_part[3].draw(pDevice, mWorld);
		tank_part[4].draw(pDevice, mWorld);
		tank_part[5].draw(pDevice, mWorld);
		tank_part[6].draw(pDevice, mWorld);
	}

	D3DXVECTOR3 getCenter(void) const
	{
		return tank_part[0].getCenter();
	}

	D3DXVECTOR3 getHead(void) const
	{
		return tank_part[1].getCenter();
	}

	void tankUpdate(float timeDiff, vector<CObstacle>& obstacles, Tank& otank, vector<vector<CWall> > walls)
	{
		if (!created) return;
		const float TIME_SCALE = 3.3;
		D3DXVECTOR3 cord = this->getCenter();

		float tX = cord.x + TIME_SCALE * timeDiff * m_velocity_x;
		float tZ = cord.z + TIME_SCALE * timeDiff * m_velocity_z;

		// tank가 맵을 벗어나지 않게
		if (tX >= ((WORLD_WIDTH / 2) - tank_part[0].getWidth() / 2))
			tX = (WORLD_WIDTH / 2) - tank_part[0].getWidth() / 2;

		else if (tX <= (-(WORLD_WIDTH / 2) + tank_part[0].getWidth() / 2))
			tX = -(WORLD_WIDTH / 2) + tank_part[0].getWidth() / 2;

		else if (tZ <= (-(WORLD_DEPTH / 2) + tank_part[0].getDepth() / 2))
			tZ = -(WORLD_DEPTH / 2) + tank_part[0].getDepth() / 2;

		else if (tZ >= ((WORLD_DEPTH / 2) - tank_part[0].getDepth() / 2))
			tZ = (WORLD_DEPTH / 2) - tank_part[0].getDepth() / 2;

		// tank가 장애물, 탱크, 벽에 충돌하면 정지
		this->setPosition(tX, cord.y, tZ);
		for (int i = 0; i < obstacles.size(); i++) {
			if (obstacles[i].get_created()) {
				if (tank_part[0].hasIntersected(obstacles[i])) {
					tX = cord.x;
					tZ = cord.z;
					break;
				}
			}
		}
		if (otank.tank_part[0].get_created()) {
			if (tank_part[0].hasIntersected(otank.tank_part[0])) {
				tX = cord.x;
				tZ = cord.z;
			}
		}
		for (int i = 0; i < walls.size(); i++) {
			for (int j = 0; j < walls[i].size(); j++) {
				if (tank_part[0].hasIntersected(walls[i][j])) {
					tX = cord.x;
					tZ = cord.z;
					break;
				}
			}
		}
		this->setPosition(tX, cord.y, tZ);

		if ((this->getCenter()[0] != last_coord[0] || this->getCenter()[2] != last_coord[2]) && distance > 0 && !isDistanceZero) {
			distance = distance - sqrt(pow(this->getCenter()[0] - last_coord[0], 2) + pow(this->getCenter()[2] - last_coord[2], 2));
			last_coord = this->getCenter();
		}
		if (distance <= 0) {
			isDistanceZero = TRUE;
		}
		if (isDistanceZero && distance <= 0) {
			setPower(0, 0);
			TANK_SPEED = 0.05;
			distance = 0.1;
		}

		//this->setPower(this->getVelocity_X() * DECREASE_RATE, this->getVelocity_Z() * DECREASE_RATE);
		//double rate = 1 - (1 - TANK_VELOCITY_RATE) * timeDiff * 400;
		double rate = 1;
		if (rate < 0)
			rate = 0;
		this->setPower(getVelocity_X() * rate, getVelocity_Z() * rate);//중력 설정 다시 손 봐야함
	}

	double getVelocity_X() { return this->m_velocity_x; }
	double getVelocity_Z() { return this->m_velocity_z; }


	void setPower(double vx, double vz)
	{
		this->m_velocity_x = vx;
		this->m_velocity_z = vz;

	}

	void setDistance() {
		distance = TANK_DISTANCE;
	}
	void setLastCoord(D3DXVECTOR3 pos) {
		last_coord = pos;
	}

	float getDistance() {
		return distance;
	}
};

// -----------------------------------------------------------------------------
// CBlueBall class definition
// -----------------------------------------------------------------------------
class CBlueBall : public CSphere
{
private:
	double radius; // 최대 반지름
	Tank* linkedTank; // 연결된 탱크
	double tankLastX, tankLastZ; // 탱크의 이전 좌표
public:
	CBlueBall(void) {
		radius = MAX_BLUEBALL_RADIUS;
	}
	double getDistanceFromTank() {
		// 파란공에서 탱크까지의 거리
		double x = getCenter().x,
			y = getCenter().y,
			z = getCenter().z;
		double tx = linkedTank->getCenter().x,
			ty = 0,
			tz = linkedTank->getCenter().z;
		return sqrt(pow(x - tx, 2) + pow(y - ty, 2) + pow(z - tz, 2));
	}
	bool hasIntersected(CSphere& ball)
	{
		return false;
	}

	double getDistanceFromTank(double new_x, double new_y, double new_z) {
		// 인자로 받은 좌표에서 탱크까지의 거리
		double tx = linkedTank->getCenter().x,
			ty = 0,
			tz = linkedTank->getCenter().z;
		return sqrt(pow(new_x - tx, 2) + pow(new_y - ty, 2) + pow(new_z - tz, 2));
	}

	double getDistanceFromTank2D() {
		// 파란공에서 탱크까지의 거리 (땅 기준)
		double x = getCenter().x,
			z = getCenter().z;
		double tx = linkedTank->getCenter().x,
			tz = linkedTank->getCenter().z;
		return sqrt(pow(x - tx, 2) + pow(z - tz, 2));
	}

	double getDistanceFromTank2D(double new_x, double new_z) {
		// 인자로 받은 좌표에서 탱크까지의 거리 (땅 기준)
		double tx = linkedTank->getCenter().x,
			tz = linkedTank->getCenter().z;
		return sqrt(pow(new_x - tx, 2) + pow(new_z - tz, 2));
	}

	double getRadius() { return radius; }
	double getMaxRadius() { return radius; }
	void setRadius(double r) { if (r > 0) radius = r; }
	//void setMaxRadius(double r) { if (r > 0)maxRadius = r; }
	void linkTank(Tank* const t) {
		linkedTank = t;
		tankLastX = linkedTank->getCenter().x;
		tankLastZ = linkedTank->getCenter().z;
	}


	void ballUpdate(float timeDiff)
	{
		if (!created) return;
		const float TIME_SCALE = 3.3;
		D3DXVECTOR3 cord = this->getCenter();
		double vx = abs(this->getVelocity_X());
		double vy = abs(this->getVelocity_Y());
		double vz = abs(this->getVelocity_Z());

		double tankX = linkedTank->getCenter().x;
		double tankZ = linkedTank->getCenter().z;

		float tX = cord.x + TIME_SCALE * timeDiff * m_velocity_x;
		float tY = cord.y + TIME_SCALE * timeDiff * m_velocity_y;
		float tZ = cord.z + TIME_SCALE * timeDiff * m_velocity_z;

		// y가 0 이하로 떨어지지 않도록 (임시)
		if (tY < 0 + M_RADIUS)
			tY = M_RADIUS;

		// 위치 설정
		// 탱크가 움직이면 blueball도 움직임
		double tankdX = tankX - tankLastX;
		double tankdZ = tankZ - tankLastZ;
		tX += tankdX;
		tZ += tankdZ;
		this->setCenter(tX, tY, tZ);

		// 속도 설정
		double rate = 1;
		this->setPower(getVelocity_X() * rate, getVelocity_Y() * rate, getVelocity_Z() * rate);

		// 파란공이 범위 벗어나면, 해당 방향 속도 0으로 만든다
		double diffFromTankX = fabs(tankX - tX);
		double diffFromTankZ = fabs(tankZ - tZ);
		if (diffFromTankX > MAX_BLUEBALL_WIDTH) {
			// X값이 벗어난 경우
			this->setPower(0, getVelocity_Y() * rate, getVelocity_Z() * rate);

			// 현재 위치에서 아주 조금 옆으로 움직여줌
			double epsilon = 0.00001;
			if (tX > tankX) epsilon *= -1;
			this->setCenter(tX + epsilon, tY, tZ);
		}
		if (diffFromTankZ > MAX_BLUEBALL_RADIUS || diffFromTankZ < MIN_BLUEBALL_RADIUS) {
			// Z값이 벗어난 경우
			this->setPower(getVelocity_X() * rate, getVelocity_Y() * rate, 0);
			// 현재 위치에서 아주 조금 앞뒤로 움직여줌
			double epsilon = 0.00001;
			if (tZ > tankZ) epsilon *= -1;
			this->setCenter(tX, tY, tZ + epsilon);
		}

		tankLastX = tankX;
		tankLastZ = tankZ;
	}

};

// -----------------------------------------------------------------------------
// Global variables
// -----------------------------------------------------------------------------
CWall	g_legoPlane;
vector<CWall> lwall1;
vector<CWall> swall1;
vector<CWall> lwall2;
vector<CWall> swall2;
vector<vector<CWall> > g_legoWall;

CBlueBall	g_target_blueball;
CLight	g_light;
CLight	g_light2;
Tank tank(0);
Tank otank(1);
bool winner;
CWall podium;

CObstacle obstacle; // 장애물 (테스트용)
CObstacle obstacle1;
vector<CObstacle> obstacles;	// 랜덤 장애물 모음
vector<CObstacle> obstacle_wall; // 장애물 (벽)

CSphere missile;   // c 누르면 나가는 미사일
ID3DXFont* DEGREEfont = NULL;
ID3DXFont* FIREDISTANCEfont = NULL;
ID3DXFont* TITLEfont = NULL;
ID3DXFont* TIMEfont = NULL; // 글자 출력을 위한 객체
ID3DXFont* ENDfont = NULL;
ID3DXFont* PLAYERfont = NULL;
ID3DXFont* DISTANCEfont = NULL;

double fireDegree = 0; // blueball - 탱크 간 각도
double fireDistance = 0; // blueball - 탱크 간 거리 (땅 기준)

D3DXVECTOR3 tankLastCoord; // 탱크 이전 프레임 위치
D3DXVECTOR3 blueballLastCoord; // bleuball 이전 프레임 위치

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

void updateFireDegree() {
	D3DXVECTOR3 targetCoord = g_target_blueball.getCenter(); // blue ball 위치
	D3DXVECTOR3 tankCoord = tank.getHead(); // 탱크 위치
	double radian = acos(
		sqrt(pow(targetCoord.x - tankCoord.x, 2) + pow(targetCoord.z - tankCoord.z, 2)) /
		sqrt(pow(targetCoord.x - tankCoord.x, 2) + pow(targetCoord.y - tankCoord.y, 2) + pow(targetCoord.z - tankCoord.z, 2))
	);
	fireDegree = radian * 180 / PI;
}

void updateFireDistance() {
	D3DXVECTOR3 targetCoord = g_target_blueball.getCenter(); // blue ball 위치
	D3DXVECTOR3 tankCoord = tank.getHead(); // 탱크 위치
	fireDistance = sqrt(pow(tankCoord.x - targetCoord.x, 2) + pow(tankCoord.z - targetCoord.z, 2));  // 땅 거리
}

bool createBlock(float partitionWidth, float partitionHeight, float partitionDepth,
	int partitionCount_x, int partitionCount_y, int partitionCount_z,
	float x, float y, float z,
	D3DXCOLOR wallColor = d3d::WHITE) {
	// (partitionCount_land * partitionCount_sky) 크기의 벽을 생성함.
	// 각 partition의 크기는 (partitionWidth, partitionHeight, partitionDepth)
	for (int i = 0; i < partitionCount_x; i++) {
		for (int j = 0; j < partitionCount_y; j++) {
			for (int k = 0; k < partitionCount_z; k++) {
				// 좌표 결정
				float nx, ny, nz;
				nx = x + partitionWidth * i;
				ny = y + partitionHeight * j;
				nz = z + partitionDepth * k;
				// 장애물 생성 & 배치
				CObstacle partition;
				if (false == partition.create(Device, -1, -1, partitionWidth, partitionHeight, partitionDepth, wallColor)) return false;
				partition.setPosition(nx, ny, nz);
				obstacle_wall.push_back(partition);
				// 전역변수에 저장
			}
		}
	}
	return true;
}

bool createDWall(float partitionWidth, float partitionHeight, float partitonDepth,
	int partitionCount_land, int partitionCount_sky,
	float x, float y, float z,
	D3DXCOLOR wallColor = d3d::WHITE) {
	// (partitionCount_land * partitionCount_sky) 크기의 벽을 생성함.
	// 각 partition의 크기는 (partitionWidth, partitionHeight, partitionDepth)
	for (int i = 0; i < partitionCount_land; i++) {
		for (int j = 0; j < partitionCount_sky; j++) {
			// 좌표 결정
			float nx, ny, nz;
			nx = x;
			ny = y + partitionHeight * j;
			nz = z + partitonDepth * i;
			// 장애물 생성 & 배치
			CObstacle partition;
			if (false == partition.create(Device, -1, -1, partitionWidth, partitionHeight, partitonDepth, wallColor)) return false;
			partition.setPosition(nx, ny, nz);
			obstacle_wall.push_back(partition);
			// 전역변수에 저장
		}
	}
	return true;
}

// 가로 장애물 생성
bool createWWall(float partitionWidth, float partitionHeight, float partitonDepth,
	int partitionCount_land, int partitionCount_sky,
	float x, float y, float z,
	D3DXCOLOR wallColor = d3d::WHITE) {
	// (partitionCount_land * partitionCount_sky) 크기의 벽을 생성함.
	// 각 partition의 크기는 (partitionWidth, partitionHeight, partitionDepth)
	for (int i = 0; i < partitionCount_land; i++) {
		for (int j = 0; j < partitionCount_sky; j++) {
			// 좌표 결정
			float nx, ny, nz;
			nx = x + partitionWidth * i;
			ny = y + partitionHeight * j;
			nz = z;
			// 장애물 생성 & 배치
			CObstacle partition;
			if (false == partition.create(Device, -1, -1, partitionWidth, partitionHeight, partitonDepth, wallColor)) return false;
			partition.setPosition(nx, ny, nz);
			obstacle_wall.push_back(partition);
			// 전역변수에 저장
		}
	}
	return true;
}


bool createWall(D3DXCOLOR wallColor) // create wall
{
	CWall wall;

	for (int i = -1; i <= 1; i += 2) {
		//앞뒤 기둥
		if (false == wall.create(Device, -1, -1, WORLD_WIDTH - 1, 2.0f, 1.0f, wallColor)) return false;
		wall.setPosition(0.0f, 1.0f, (float)i * WORLD_DEPTH / 2);
		lwall1.push_back(wall);
		//앞뒤 벽
		if (false == wall.create(Device, -1, -1, 1.0f, 2.5f, 1.5f, wallColor)) return false;
		wall.setPosition(0, 1.25f, (float)i * WORLD_DEPTH / 2);
		swall1.push_back(wall);
		//좌측 벽
		if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, WORLD_DEPTH - 1, wallColor)) return false;
		wall.setPosition((float)i * WORLD_WIDTH / 2, 1.0f, 0.0f);
		lwall2.push_back(wall);
	}

	for (int i = -1; i <= 1; i += 2) {
		for (int j = -2; j <= 2; j++) {
			//중간 기둥
			if (false == wall.create(Device, -1, -1, 1.5f, 2.5f, 2.0f, wallColor)) return false;
			wall.setPosition((float)i * WORLD_WIDTH / 2, 1.25f, (float)j * WORLD_DEPTH / 6);
			swall2.push_back(wall);
		}

		for (int j = -1; j <= 1; j += 2) {
			//꼭짓점 기둥
			if (false == wall.create(Device, -1, -1, 1.5f, 3.0f, 1.5f, wallColor)) return false;
			wall.setPosition((float)i * WORLD_WIDTH / 2, 1.5f, (float)j * WORLD_DEPTH / 2);
			swall2.push_back(wall);
		}
	}

	g_legoWall.push_back(lwall1);
	g_legoWall.push_back(lwall2);
	g_legoWall.push_back(swall1);
	g_legoWall.push_back(swall2);

	return true;
}

bool createMap()
{
	float w = WORLD_WIDTH;
	float d = WORLD_DEPTH;

	// 벽
	createWall(d3d::WHITE);

	// 바닥
	if (false == g_legoPlane.create(Device, -1, -1, WORLD_WIDTH, 0.03f, WORLD_DEPTH, d3d::WHITER_SAND)) return false;
	g_legoPlane.setPosition(0.0f, -0.0006f / 5, 0.0f);

	// 장애물
	createWWall(0.4f, 0.7f, 1.0f, 18, 3, -w / 2 + 0.85f, 0.35f, d / 6, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, -w / 2 + 7.95f, 0.25f, d / 6, d3d::LIGHTGRAY);
	createDWall(0.4f, 0.7f, 1.0f, 10, 3, -w / 2 + 7.95f, 0.35f, d / 6 - 10.25f, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, -w / 2 + 7.95f, 0.25f, d / 6 - 11.5f, d3d::LIGHTGRAY);

	createWWall(0.4f, 0.7f, 1.0f, 18, 3, -w / 2 + 0.85f, 0.35f, -d / 6, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, -w / 2 + 7.95f, 0.25f, -d / 6, d3d::LIGHTGRAY);
	createDWall(0.4f, 0.7f, 1.0f, 10, 3, -w / 2 + 7.95f, 0.35f, -d / 6 + 1.25f, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, -w / 2 + 7.95f, 0.25f, -d / 6 + 11.25f, d3d::LIGHTGRAY);

	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 0, 0.25f, -d / 3, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.7f, 1.0f, 15, 3, 0.95f, 0.35f, -d / 3, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 7.5f, 0.25f, -d / 3, d3d::LIGHTGRAY);
	createDWall(0.8f, 0.7f, 1.0f, 25, 3, 7.5f, 0.35f, -d / 3 + 1.25f, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 7.5f, 0.25f, -d / 3 + 26.5f, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.7f, 1.0f, 15, 3, 0.95f, 0.35f, -d / 3 + 26.5f, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 0, 0.25f, -d / 3 + 26.5f, d3d::LIGHTGRAY);

	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 0, 0.25f, d / 3, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.7f, 1.0f, 15, 3, 0.95f, 0.35f, d / 3, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 7.5f, 0.25f, d / 3, d3d::LIGHTGRAY);
	createDWall(0.8f, 0.7f, 1.0f, 25, 3, 7.5f, 0.35f, d / 3 - 25.25f, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 7.5f, 0.25f, d / 3 - 26.5f, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.7f, 1.0f, 15, 3, 0.95f, 0.35f, d / 3 - 26.5f, d3d::LIGHTGRAY);
	createWWall(1.5f, 0.5f, 1.5f, 1, 5, 0, 0.25f, d / 3 - 26.5f, d3d::LIGHTGRAY);


	createWWall(1.2f, 0.5f, 1.2f, 1, 8, -2.0f, 0.25f, 3.0f, d3d::LIGHTGRAY);
	createWWall(1.2f, 0.5f, 1.2f, 1, 8, -2.0f, 0.25f, -3.0f, d3d::LIGHTGRAY);
	createWWall(1.2f, 0.5f, 1.2f, 1, 7, -1.0f, 0.25f, 4.0f, d3d::LIGHTGRAY);
	createWWall(1.2f, 0.5f, 1.2f, 1, 7, -1.0f, 0.25f, -4.0f, d3d::LIGHTGRAY);
	createWWall(1.2f, 0.5f, 1.2f, 1, 9, -3.0f, 0.25f, 1.5f, d3d::LIGHTGRAY);
	createWWall(1.2f, 0.5f, 1.2f, 1, 9, -3.0f, 0.25f, -1.5f, d3d::LIGHTGRAY);

	// 경기장 모형
	createWWall(1.2f, 0.8f, 1.2f, 1, 5, 3.0f, 0.4f, 3.0f, d3d::LIGHTGRAY);

	createWWall(0.4f, 0.4f, 0.5f, 10, 1, 3.8f, 1.2f, 3.0f, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.4f, 0.5f, 10, 1, 3.8f, 2.0f, 3.0f, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.4f, 0.5f, 10, 1, 3.8f, 2.8f, 3.0f, d3d::LIGHTGRAY);

	createWWall(1.2f, 0.8f, 1.2f, 1, 5, 8.2f, 0.4f, 3.0f, d3d::LIGHTGRAY);

	createDWall(0.4f, 0.4f, 0.4f, 12, 1, 8.2f, 1.2f, -2.2f, d3d::LIGHTGRAY);
	createDWall(0.4f, 0.4f, 0.4f, 12, 1, 8.2f, 2.0f, -2.2f, d3d::LIGHTGRAY);
	createDWall(0.4f, 0.4f, 0.4f, 12, 1, 8.2f, 2.8f, -2.2f, d3d::LIGHTGRAY);

	createWWall(1.2f, 0.8f, 1.2f, 1, 5, 8.2f, 0.4f, -3.0f, d3d::LIGHTGRAY);

	createWWall(0.4f, 0.4f, 0.5f, 10, 1, 3.8f, 1.2f, -3.0f, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.4f, 0.5f, 10, 1, 3.8f, 2.0f, -3.0f, d3d::LIGHTGRAY);
	createWWall(0.4f, 0.4f, 0.5f, 10, 1, 3.8f, 2.8f, -3.0f, d3d::LIGHTGRAY);

	createWWall(1.2f, 0.8f, 1.2f, 1, 5, 3.0f, 0.4f, -3.0f, d3d::LIGHTGRAY);

	createDWall(0.4f, 0.4f, 0.4f, 12, 1, 3.0f, 1.2f, -2.2f, d3d::LIGHTGRAY);
	createDWall(0.4f, 0.4f, 0.4f, 12, 1, 3.0f, 2.0f, -2.2f, d3d::LIGHTGRAY);
	createDWall(0.4f, 0.4f, 0.4f, 12, 1, 3.0f, 2.8f, -2.2f, d3d::LIGHTGRAY);

	// 위 아래 ㄷ

	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 14.4f, 0.25f, -d / 4 + 13.4f, d3d::LIGHTGRAY);
	createDWall(0.5f, 0.7f, 1.0f, 12, 3, -w / 2 + 14.4f, 0.35f, -d / 4 + 1.2f, d3d::LIGHTGRAY);
	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 14.4f, 0.25f, -d / 4, d3d::LIGHTGRAY);
	createWWall(0.5f, 0.7f, 1.0f, 16, 3, -w / 2 + 5.95f, 0.25f, -d / 4, d3d::LIGHTGRAY);

	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 5.0f, 0.25f, -d / 4, d3d::LIGHTGRAY);
	createDWall(0.5f, 0.7f, 1.0f, 14, 3, -w / 2 + 5.0f, 0.35f, -d / 4 - 14.2f, d3d::LIGHTGRAY);
	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 5.0f, 0.25f, -d / 4 - 15.4f, d3d::LIGHTGRAY);
	createWWall(0.5f, 0.7f, 1.0f, 20, 3, -w / 2 + 5.95f, 0.35f, -d / 4 - 15.4f, d3d::LIGHTGRAY);
	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 16.4f, 0.25f, -d / 4 - 15.4f, d3d::LIGHTGRAY);

	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 14.4f, 0.25f, d / 4 - 13.4f, d3d::LIGHTGRAY);
	createDWall(0.5f, 0.7f, 1.0f, 12, 3, -w / 2 + 14.4f, 0.35f, d / 4 - 12.2f, d3d::LIGHTGRAY);
	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 14.4f, 0.25f, d / 4, d3d::LIGHTGRAY);
	createWWall(0.5f, 0.7f, 1.0f, 16, 3, -w / 2 + 5.95f, 0.25f, d / 4, d3d::LIGHTGRAY);

	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 5.0f, 0.25f, d / 4, d3d::LIGHTGRAY);
	createDWall(0.5f, 0.7f, 1.0f, 14, 3, -w / 2 + 5.0f, 0.35f, d / 4 + 1.2f, d3d::LIGHTGRAY);
	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 5.0f, 0.25f, d / 4 + 15.4f, d3d::LIGHTGRAY);
	createWWall(0.5f, 0.7f, 1.0f, 20, 3, -w / 2 + 5.95f, 0.25f, d / 4 + 15.4f, d3d::LIGHTGRAY);
	createWWall(1.4f, 0.5f, 1.4f, 1, 5, -w / 2 + 16.4f, 0.25f, d / 4 + 15.4f, d3d::LIGHTGRAY);


	return true;
}


void destroyAllLegoBlock(void)
{
}

// initialization
bool Setup()
{
	int i;

	// 글자출력 ---------------------
	if (FAILED(D3DXCreateFont(Device, 40, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &DEGREEfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	if (FAILED(D3DXCreateFont(Device, 40, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &FIREDISTANCEfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	if (FAILED(D3DXCreateFont(Device, 350, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &TITLEfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	if (FAILED(D3DXCreateFont(Device, 40, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &TIMEfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	if (FAILED(D3DXCreateFont(Device, 150, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &ENDfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	if (FAILED(D3DXCreateFont(Device, 50, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &PLAYERfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	if (FAILED(D3DXCreateFont(Device, 40, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &DISTANCEfont)))
	{
		::MessageBox(0, "D3DXCreateFont() - FAILED", 0, 0);
		return false;
	}
	// ------------------------------

	D3DXMatrixIdentity(&g_mWorld);
	D3DXMatrixIdentity(&g_mView);
	D3DXMatrixIdentity(&g_mProj);

	if (false == tank.create(Device, -1, -1, d3d::BROWN)) return false;
	tank.setPosition(0, 0.38f, -WORLD_DEPTH / 2 + 5);
	tank.setLastCoord(tank.getCenter());

	if (false == otank.create(Device, -1, -1, d3d::GREEN)) return false;
	otank.setPosition(0, 0.38f, WORLD_DEPTH / 2 - 5);
	otank.setLastCoord(otank.getCenter());

	// tank랑 blue ball 연결
	g_target_blueball.linkTank(&tank);

	// 벽, 바닥 생성
	createMap();
	// 장애물 생성

	// create blue ball for set direction
	if (false == g_target_blueball.create(Device, d3d::RED)) return false;
	//g_target_blueball.setCenter(.0f, (float)M_RADIUS + 3, .0f);
	g_target_blueball.setCenter(tank.getCenter().x - 0.01f, (float)M_RADIUS + 3, tank.getCenter().z + 5.0f);

	if (false == podium.create(Device, -1, -1, 2.0f, 1.2f, 2.0f, d3d::GOLD)) return false;
	podium.setPosition(0.0f, 0.6f, 0.0f);

	// light setting 
	D3DLIGHT9 lit;
	::ZeroMemory(&lit, sizeof(lit));
	lit.Type = D3DLIGHT_POINT;
	lit.Diffuse = d3d::WHITE * 1.8f;  // 원래 1배였음
	lit.Specular = d3d::WHITE * 1.5f;  //원래 0.9배였음
	lit.Ambient = d3d::WHITE * 0.9f;//원래 0.9배였음
	lit.Range = 100.0f;
	lit.Attenuation0 = 0.0f;//상수 감쇠
	lit.Attenuation1 = 0.3f;//선형감쇠 원래 0.9f였음.
	lit.Attenuation2 = 0.0f;//제곱감쇠
	lit.Position = D3DXVECTOR3(0.0f, 10.0f, WORLD_DEPTH / 4 + 4);
	if (false == g_light.create(Device, lit))
		return false;
	lit.Position = D3DXVECTOR3(0.0f, 10.0f, -WORLD_DEPTH / 4 - 4);
	if (false == g_light2.create(Device, lit))
		return false;

	// Position and aim the camera.
	D3DXVECTOR3 pos(0.0f, 5.0f, -8.0f);
	D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 up(0.0f, 2.0f, 0.0f);
	D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
	Device->SetTransform(D3DTS_VIEW, &g_mView);

	// Set the projection matrix.
	D3DXMatrixPerspectiveFovLH(&g_mProj, D3DX_PI / 4,
		(float)Width / (float)Height, 1.0f, 100.0f);
	Device->SetTransform(D3DTS_PROJECTION, &g_mProj);

	// Set render states.
	Device->SetRenderState(D3DRS_LIGHTING, TRUE);
	Device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
	Device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

	g_light.setLight(Device, g_mWorld);
	g_light2.setLight(Device, g_mWorld);
	return true;
}

void Cleanup(void)
{
	g_legoPlane.destroy();
	for (int i = 0; i < g_legoWall.size(); i++) {
		for (int j = 0; j < g_legoWall[i].size(); j++)
			g_legoWall[i][j].destroy();
	}
	destroyAllLegoBlock();
	g_light.destroy();

	// 글자출력 ----------------------------
	if (TIMEfont != NULL)
	{
		TIMEfont->Release();
		TIMEfont = NULL;
	}
	//--------------------------------------

}

float x_camera = 0.0f;
float y_camera = 0.8f;

float back_camera = 1;
float camera_prefix = 0.05f;

bool isOriginTank = TRUE;

bool isFire = FALSE;

int turnTime = 20000;

bool threeTime = FALSE;
bool zoomOutTiming = FALSE; //정재민

double startTime = (double)timeGetTime();
double currTime = (double)timeGetTime();
double timediff = currTime - startTime;

float zoomOutSpeed = 0.0; //정재민


// timeDelta represents the time between the current image frame and the last image frame.
// the distance of moving balls should be "velocity * timeDelta"
bool Display(float timeDelta)
{
	int i = 0;
	int j = 0;
	D3DXVECTOR3 pos;
	D3DXVECTOR3 target;
	D3DXVECTOR3 up;

	if (!missile.getCreated()) {
		currTime = (double)timeGetTime();
		timediff = currTime - startTime;
	}

	if (missile.getCreated()) {
		isFire = TRUE;
		tank.setPower(0, 0);
	}

	if (isFire && !missile.getCreated() && !threeTime) { /////////////
		startTime = currTime;
		timediff = 0;
		turnTime = 3000;
		threeTime = TRUE;
		zoomOutTiming = TRUE;

	}

	if (GAME_START == false) {
		pos = D3DXVECTOR3(20.0f, 12.0f, -WORLD_DEPTH / 2 + MOVEMENT);
		target = D3DXVECTOR3(0.0f, 0.0f, -WORLD_DEPTH / 2 + MOVEMENT);
		up = D3DXVECTOR3(0.0f, 2.0f, 0.0f);



		MOVEMENT = MOVEMENT + 0.015;
		startTime = currTime;
		if (MOVEMENT > WORLD_DEPTH) {
			GAME_START = true;
		}
		//D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
		//Device->SetTransform(D3DTS_VIEW, &g_mView);

	}
	else if (GAME_FINISH) {
		tank.setPosition(0.0f, podium.getCenter()[1] + podium.getHeight() / 2 + 0.40, 0.0f);
		pos = D3DXVECTOR3(0.0f, tank.getHead()[1] + 0.5f, -5 + 10 * isOriginTank);
		target = D3DXVECTOR3(0.0f, tank.getHead()[1], 0.0f);
		up = D3DXVECTOR3(0.0f, 2.0f, 0.0f);
		D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
		Device->SetTransform(D3DTS_VIEW, &g_mView);
		if (Device)
		{
			Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00afafaf, 1.0f, 0);
			Device->BeginScene();
			RECT screenRect;
			GetClientRect(GetDesktopWindow(), &screenRect);
			RECT rect = { screenRect.right / 2-180, Height / 7, screenRect.right, screenRect.bottom };
			ENDfont->DrawText(NULL, "Winner", -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			rect = { screenRect.right / 2-85, Height / 7 + 150,screenRect.right, screenRect.bottom};
			if (isOriginTank) {
				PLAYERfont->DrawText(NULL, "PLAYER1", -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			}
			else {
				PLAYERfont->DrawText(NULL, "PLAYER2", -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			}
			g_legoPlane.draw(Device, g_mWorld);
			for (int i = 0; i < g_legoWall.size(); i++)
			{
				for (int j = 0; j < g_legoWall[i].size(); j++)
					g_legoWall[i][j].draw(Device, g_mWorld);
			}
			podium.draw(Device, g_mWorld);
			tank.draw(Device, g_mWorld);
			Device->EndScene();
			Device->Present(0, 0, 0, 0);
			Device->SetTexture(0, NULL);
		}
		return true;

	}
	else if (isFire == true) {
		pos = D3DXVECTOR3(missile.getCenter()[0], missile.getCenter()[1] + 0.9f, missile.getCenter()[2] + 1.5f - 3.0f * isOriginTank);
		target = D3DXVECTOR3(missile.getCenter()[0], missile.getCenter()[1], missile.getCenter()[2]);
	}
	else {
		if (camera_option == 0) {
			if (isOriginTank) {
				pos = D3DXVECTOR3(tank.getHead()[0], tank.getHead()[1] + 2.0f, tank.getHead()[2] - back_camera * 4.4f);
			}
			else {
				pos = D3DXVECTOR3(tank.getHead()[0], tank.getHead()[1] + 2.0f, tank.getHead()[2] + back_camera * 4.4f);
			}

			target = D3DXVECTOR3(tank.getHead()[0] + x_camera, tank.getHead()[1] + y_camera, tank.getHead()[2]);
		}
		else if (camera_option == 1) {
			pos = D3DXVECTOR3(50.0, 20.0, 0.0);
			target = D3DXVECTOR3(0, 1, 0);
		}
		else {
			pos = D3DXVECTOR3(tank.getHead()[0], tank.getHead()[1] + 0.5f, tank.getHead()[2] + 0.4f - 0.8f * isOriginTank);
			if (abs(tank.getHead()[2] - g_target_blueball.getCenter()[2]) < 5) {
				target = D3DXVECTOR3(g_target_blueball.getCenter()[0], g_target_blueball.getCenter()[1], g_target_blueball.getCenter()[2]);
			}
			else {
				target = D3DXVECTOR3(g_target_blueball.getCenter()[0], g_target_blueball.getCenter()[1] - 2.0f, g_target_blueball.getCenter()[2]);
			}
		}
	}

	if (zoomOutTiming) { //정재민
		pos = D3DXVECTOR3(missile.getCenter()[0], missile.getCenter()[1] + 0.9f + zoomOutSpeed, missile.getCenter()[2] + 1.5f - 3.0f * isOriginTank);
		target = D3DXVECTOR3(missile.getCenter().x, missile.getCenter().y, missile.getCenter().z);
		zoomOutSpeed += 0.0018f;
	}

	up = D3DXVECTOR3(0.0f, 2.0f, 0.0f);
	D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
	Device->SetTransform(D3DTS_VIEW, &g_mView);


	if (Device)
	{

		if (timediff > turnTime) {
			tank.setPower(0, 0);
			tank.setIsDistanceZero(FALSE);
			Tank tempTank = tank;
			tank = otank;
			otank = tempTank;
			g_target_blueball.linkTank(&tank);
			camera_option = 0;
			back_camera = 1;
			otank.setDistance();
			x_camera = 0.0f;
			y_camera = 0.5f;
			isFire = FALSE;
			threeTime = FALSE;
			turnTime = 20000;
			TANK_SPEED = 0.45; //정재민
			zoomOutTiming = FALSE; //정재민
			zoomOutSpeed = 0.0; //정재민

			startTime = currTime;

			if (isOriginTank) {
				g_target_blueball.setCenter(tank.getCenter().x - 0.01f, (float)M_RADIUS + 3, tank.getCenter().z - 5.0f);
				// 0.01f 빼는 이유는 사분면 구분에서 0이 포함되는 경우가 중첩되어 x의 차이가 0인 경우 오류가 발생하기 때문.
				isOriginTank = FALSE;
			}
			else {
				g_target_blueball.setCenter(tank.getCenter().x - 0.01f, (float)M_RADIUS + 3, tank.getCenter().z + 5.0f);
				isOriginTank = TRUE;
			}
		}

		Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00afafaf, 1.0f, 0);
		Device->BeginScene();

		
		// 글자출력-------------------------------------------------------------------------------------
		if (GAME_START) {
			RECT rect = { 10, 10, 0, 0 };  // 글자의 위치 (10, 10)에서 시작
			if ((turnTime / 1000) - static_cast<int>(timediff / 1000) > 5) {
				string time = "TIME: " + to_string((turnTime / 1000) - static_cast<int>(timediff / 1000));
				TIMEfont->DrawText(NULL, time.c_str(), -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			}
			else {
				string time = "TIME: " + to_string((turnTime / 1000) - static_cast<int>(timediff / 1000));
				TIMEfont->DrawText(NULL, time.c_str(), -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(255, 0, 0));
			}
		}
		if (GAME_START && !isFire) {
			RECT rect = { 10, 50, 0, 0 };
			ostringstream oss;
			oss << "FIRE Degree: " << fixed << setprecision(2) << fireDegree << "°";
			string s = oss.str();
			DEGREEfont->DrawText(NULL, s.c_str(), -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			rect = { 10, 90, 0, 0 };
			oss.str("");  // 스트림 비움
			oss << "FIRE Distance: " << std::fixed << std::setprecision(2) << fireDistance;
			s = oss.str();  //발사거리 문자열로
			FIREDISTANCEfont->DrawText(NULL, s.c_str(), -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			rect = { 10, 130, 0, 0 };
			if (tank.getIsDistanceZero()) {
				DISTANCEfont->DrawText(NULL, "Tank: SLOWED", -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(255, 0, 0));
			}
			else {
				DISTANCEfont->DrawText(NULL, ("Tank Distance: " + to_string(int(tank.getDistance()))).c_str(), -1, &rect, DT_NOCLIP, D3DCOLOR_XRGB(0, 0, 0));
			}
			
		}


		//----------------------------------------------------------------------------------------------


		// 탱크 위치 변경
		tank.tankUpdate(timeDelta, obstacle_wall, otank, g_legoWall);
		// 미사일 위치도 변경 & 벽과 충돌했는지 체크
		missile.ballUpdate(timeDelta);
		for (i = 0; i < g_legoWall.size(); i++) {
			for (j = 0; j < g_legoWall[i].size(); j++)
				g_legoWall[i][j].hitBy(missile);
		}

		// 블루볼 위치 변경
		g_target_blueball.ballUpdate(timeDelta);

		// draw plane, walls, and spheres
		tank.draw(Device, g_mWorld);
		g_target_blueball.draw(Device, g_mWorld);
		missile.draw(Device, g_mWorld);  // 미사일도 그림

		g_legoPlane.draw(Device, g_mWorld);
		for (int i = 0; i < g_legoWall.size(); i++)
		{
			for (int j = 0; j < g_legoWall[i].size(); j++)
				g_legoWall[i][j].draw(Device, g_mWorld);
		}

		if (missile.get_created() == true) {
			missile.hitBy();
		}
		if (otank.get_created()) {
			if (otank.hasIntersected(missile)) {
				otank.hitBy(missile);
				GAME_FINISH = true;
				winner = true;
			}
			else if (otank.get_created()) {
				otank.tankUpdate(timeDelta, obstacle_wall, tank, g_legoWall);
				otank.draw(Device, g_mWorld);
			}
		}

		if (otank.get_created()) {
			if (otank.hasIntersected(missile)) {
				otank.hitBy(missile);
				GAME_FINISH = true;
				winner = true;
			}
			else if (otank.get_created()) {
				otank.tankUpdate(timeDelta, obstacle_wall, tank, g_legoWall);
				otank.draw(Device, g_mWorld);
			}
		}

		// 장애물(벽) 파괴 체크 & 파괴 안되면 그림
		for (int i = 0; i < obstacle_wall.size(); i++) {
			if (obstacle_wall[i].get_created()) {
				if (obstacle_wall[i].hasIntersected(missile)) {
					obstacle_wall[i].hitBy(missile);
					// 만약 장애물 파괴시, 폭발한다 (= 주변 장애물도 다시 그림)
					// 스파게티 코드지만 마감 얼마 안남았으니까 이대로 갑시다
					for (int j = 0; j < obstacle_wall.size(); j++) {
						if (obstacle_wall[j].hasIntersected(missile.getCenter().x, missile.getCenter().y, missile.getCenter().z, MISSILE_EXPOLSION_RADIUS)) {
							obstacle_wall[j].hitBy(missile);
						}
					}
				}
				else if (obstacle_wall[i].get_created()) {
					obstacle_wall[i].draw(Device, g_mWorld);
				}
			}
		}

		//g_light.draw(Device);

		D3DXVECTOR3 tankCoord = tank.getHead();
		D3DXVECTOR3 blueballCoord = g_target_blueball.getCenter();
		if (tankCoord != tankLastCoord || blueballCoord != blueballLastCoord) {
			// 탱크나 블루볼 움직였으면, 각도 및 거리 재계산
			updateFireDegree();
			updateFireDistance();
		}
		tankLastCoord = tankCoord;
		blueballLastCoord = blueballCoord;
		if (GAME_START == false) {
			// 화면 크기 얻기
			RECT screenRect;
			GetClientRect(GetDesktopWindow(), &screenRect);
			// 화면 중앙에 텍스트 출력
			RECT rect = { 0, screenRect.bottom / 4, screenRect.right, screenRect.bottom };
			TITLEfont->DrawText(NULL, "Tank Game", -1, &rect, DT_CENTER, D3DCOLOR_XRGB(0, 0, 0));
		}



		Device->EndScene();
		Device->Present(0, 0, 0, 0);
		Device->SetTexture(0, NULL);
	}
	return true;
}

LRESULT CALLBACK d3d::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static bool wire = false;
	static bool isReset = true;
	static int old_x = 0;
	static int old_y = 0;
	static enum { WORLD_MOVE, LIGHT_MOVE, BLOCK_MOVE } move = WORLD_MOVE;

	switch (msg) {
		/*
		'W' 키: VK_W (0x57)
		'S' 키: VK_S (0x53)
		'A' 키: VK_A (0x41)
		'D' 키: VK_D (0x44)
		*/
	case WM_DESTROY:
	{
		::PostQuitMessage(0);
		break;
	}
	case WM_KEYDOWN:
	{
		switch (wParam) {
		case VK_ESCAPE:
			// esc 누름
			::DestroyWindow(hwnd);
			break;
		case VK_RETURN:
			// 엔터 누름
			// 게임 시작 전일 경우, 게임 시작하고 끝
			if (!GAME_START) {
				GAME_START = true;
				break;
			}
			// 물체 렌더링 상태 보여줌
			if (NULL != Device) {
				wire = !wire;
				Device->SetRenderState(D3DRS_FILLMODE,
					(wire ? D3DFILL_WIREFRAME : D3DFILL_SOLID));
			}
			break;
		case VK_SPACE:
		{	// 스페이스바 누름
			// 파란 공 쪽으로 미사일 발사
			if (!isFire && GAME_START) {
				D3DXVECTOR3 targetpos = g_target_blueball.getCenter();
				D3DXVECTOR3	whitepos = tank.getHead();
				double theta = acos(
					sqrt(pow(targetpos.x - whitepos.x, 2)) /
					sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2))
				);		// 기본 1 사분면
				if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x >= 0) { theta = -theta; }	//4 사분면
				if (targetpos.z - whitepos.z >= 0 && targetpos.x - whitepos.x <= 0) { theta = PI - theta; } //2 사분면
				if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x <= 0) { theta = PI + theta; } // 3 사분면
				/*
				double distance_land = sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2)); // xz만 고려한 거리

				double theta_sky = acos(
					sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2)) /
					sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.y - whitepos.y, 2) + pow(targetpos.z - whitepos.z, 2))
				);
				*/
				double distance_land = fireDistance;  // 전역변수에서 가져옴
				double theta_sky = fireDegree * PI / 180;  // 전역변수에서 가져옴
				double distance_sky = sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.y - whitepos.y, 2) + pow(targetpos.z - whitepos.z, 2));  // y좌표 고려한 거리
				//double distance = sqrt( sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2)) + pow(targetpos.y - whitepos.y, 2)); // y좌표 포함 계산

				missile.destroy();
				missile.create(Device, d3d::BLACK);
				missile.setCenter(whitepos.x, whitepos.y, whitepos.z);
				missile.setPower(distance_land * cos(theta) * MISSILE_POWER, distance_sky * sin(theta_sky), distance_land * sin(theta) * MISSILE_POWER);
				break;
			}
			// 게임 시작 전일 경우, 게임 시작하고 끝
			if (!GAME_START) {
				GAME_START = true;
			}
			break;
		}

		case VK_LEFT:
		{
			if (!isFire && GAME_START) {
				// 키보드 좌측 버튼
				CBlueBall* moveTarget = &g_target_blueball;
				Tank* shooter = &tank;
				double velocity = -1 * BLUEBALL_VELOCITY;
				D3DXVECTOR3 tankcoord = shooter->getCenter();
				D3DXVECTOR3 targetcoord = moveTarget->getCenter();
				double dx = tankcoord.x - targetcoord.x;
				if (!isOriginTank) {
					velocity *= -1;
					dx *= -1;
				}
				if (dx < MAX_BLUEBALL_WIDTH) moveTarget->setPower(velocity, moveTarget->getVelocity_Y(), moveTarget->getVelocity_Z());
			}
			break;
		}

		case VK_RIGHT:
		{
			if (!isFire && GAME_START) {
				// 키보드 우측 버튼
				CBlueBall* moveTarget = &g_target_blueball;
				Tank* shooter = &tank;
				double velocity = BLUEBALL_VELOCITY;
				D3DXVECTOR3 tankcoord = shooter->getCenter();
				D3DXVECTOR3 targetcoord = moveTarget->getCenter();
				double dx = targetcoord.x - tankcoord.x;
				if (!isOriginTank) {
					velocity *= -1;
					dx *= -1;
				}
				if (dx < MAX_BLUEBALL_WIDTH) moveTarget->setPower(velocity, moveTarget->getVelocity_Y(), moveTarget->getVelocity_Z());
			}
			break;
		}

		case VK_UP:
		{
			if (!isFire && GAME_START) {
				// 키보드 위측 버튼
				CBlueBall* moveTarget = &g_target_blueball;
				Tank* shooter = &tank;
				double velocity = BLUEBALL_VELOCITY;
				D3DXVECTOR3 tankcoord = shooter->getCenter();
				D3DXVECTOR3 targetcoord = moveTarget->getCenter();
				double dz = targetcoord.z - tankcoord.z;
				if (!isOriginTank) {
					velocity *= -1;
					dz *= -1;
				}
				if (dz < MAX_BLUEBALL_RADIUS)
					moveTarget->setPower(moveTarget->getVelocity_X(), moveTarget->getVelocity_Y(), velocity);
			}
			break;
		}

		case VK_DOWN:
		{
			if (!isFire && GAME_START) {
				// 키보드 아래측 버튼
				CBlueBall* moveTarget = &g_target_blueball;
				Tank* shooter = &tank;
				double velocity = -BLUEBALL_VELOCITY;
				D3DXVECTOR3 tankcoord = shooter->getCenter();
				D3DXVECTOR3 targetcoord = moveTarget->getCenter();
				double dz = targetcoord.z - tankcoord.z;
				if (!isOriginTank) {
					velocity *= -1;
					dz *= -1;
				}
				if (dz > MIN_BLUEBALL_RADIUS) moveTarget->setPower(moveTarget->getVelocity_X(), moveTarget->getVelocity_Y(), velocity);
			}
			break;
		}

		// 재환이 + 나 버전

		case 0x57:
		{
			if (!isFire && GAME_START) {
				// W
				Tank* moveTarget = &tank;  // 움직일 대상
				double speed = TANK_SPEED;
				if (isOriginTank) {
					moveTarget->setPower(moveTarget->getVelocity_X(), speed * 5);
				}
				else {
					moveTarget->setPower(moveTarget->getVelocity_X(), -speed * 5);
				}
			}
			break;
		}

		case 0x41:
		{
			if (!isFire && GAME_START) {
				// A
				Tank* moveTarget = &tank;  // 움직일 대상
				double speed = TANK_SPEED;
				if (isOriginTank) {
					moveTarget->setPower(-speed * 5, moveTarget->getVelocity_Z());
				}
				else {
					moveTarget->setPower(speed * 5, moveTarget->getVelocity_Z());
				}
			}
			break;
		}

		case 0x53:
		{
			if (!isFire && GAME_START) {
				// S
				Tank* moveTarget = &tank;  // 움직일 대상
				double speed = TANK_SPEED;
				if (isOriginTank) {
					moveTarget->setPower(moveTarget->getVelocity_X(), -speed * 5);
				}
				else {
					moveTarget->setPower(moveTarget->getVelocity_X(), speed * 5);
				}

			}
			break;
		}
		case 0x44:
		{
			if (!isFire && GAME_START) {
				// D
				Tank* moveTarget = &tank;;  // 움직일 대상
				double speed = TANK_SPEED;
				if (isOriginTank) {
					moveTarget->setPower(speed * 5, moveTarget->getVelocity_Z());
				}
				else {
					moveTarget->setPower(-speed * 5, moveTarget->getVelocity_Z());
				}
			}
			break;
		}
		case 0x56:
		{
			if (GAME_START) {
				// v 버튼 누를 시
				if (camera_option == 0) { camera_option = 1; }
				else if (camera_option == 1) { camera_option = 2; }
				else { camera_option = 0; }
			}
			break;
		}
		case 0x10:
		case 0x51:
		{
			if (!isFire && GAME_START) {
				// Shift, Q키
				// blueball 올림
				CBlueBall* moveTarget = &g_target_blueball;
				double velocity = BLUEBALL_VELOCITY;
				moveTarget->setPower(moveTarget->getVelocity_X(), velocity, moveTarget->getVelocity_Z());
			}
			break;
		}
		case 0x45:
		case 0x11:
		{
			if (!isFire && GAME_START) {
				// Ctrl키, E키
				// blueball 내림
				CBlueBall* moveTarget = &g_target_blueball;
				double velocity = -1 * BLUEBALL_VELOCITY;
				moveTarget->setPower(moveTarget->getVelocity_X(), velocity, moveTarget->getVelocity_Z());
			}
			break;
		}

		case 0x43:
		{
			if (GAME_START) {
				back_camera = back_camera * -1;
			}
			break;
		}

		case 0x61:
		{
			if (GAME_START) {
				if (isOriginTank) {
					x_camera = x_camera - camera_prefix;
				}
				else {
					x_camera = x_camera + camera_prefix;
				}
				y_camera = y_camera - camera_prefix;
			}
			break;
		}
		case 0x62:
		{
			if (GAME_START) {
				y_camera = y_camera - camera_prefix;
			}
			break;
		}
		case 0x63:
		{
			if (GAME_START) {
				if (isOriginTank) {
					x_camera = x_camera + camera_prefix;
				}
				else {
					x_camera = x_camera - camera_prefix;
				}
				y_camera = y_camera - camera_prefix;

			}
			break;
		}
		case 0x64:
		{
			if (GAME_START) {
				if (isOriginTank) {
					x_camera = x_camera - camera_prefix;
				}
				else {
					x_camera = x_camera + camera_prefix;
				}
			}
			break;
		}
		case 0x65:
		{
			if (GAME_START) {
				x_camera = 0.0f;
				y_camera = 0.5f;
			}
			break;
		}
		case 0x66:
		{
			if (GAME_START) {
				if (isOriginTank) {
					x_camera = x_camera + camera_prefix;
				}
				else {
					x_camera = x_camera - camera_prefix;
				}
			}
			break;
		}
		case 0x67:
		{
			if (GAME_START) {
				if (isOriginTank) {
					x_camera = x_camera - camera_prefix;
				}
				else {
					x_camera = x_camera + camera_prefix;
				}
				y_camera = y_camera + camera_prefix;
			}
			break;
		}
		case 0x68:
		{
			if (GAME_START) {
				y_camera = y_camera + camera_prefix;
			}
			break;
		}
		case 0x69:
		{
			if (GAME_START) {
				if (isOriginTank) {
					x_camera = x_camera + camera_prefix;
				}
				else {
					x_camera = x_camera - camera_prefix;
				}
				y_camera = y_camera + camera_prefix;
			}
			break;
		}

		}
		break;
	}
	case WM_KEYUP:
	{
		switch (wParam) {
		case 0x44:
		case 0x41:
		{
			// A, D키 뗌	
			Tank* moveTarget = &tank;  // 움직일 대상
			moveTarget->setPower(0, moveTarget->getVelocity_Z());
			break;
		}
		case 0x57:
		case 0x53:
		{
			// W, S키 뗌
			Tank* moveTarget = &tank;  // 움직일 대상
			moveTarget->setPower(moveTarget->getVelocity_X(), 0);
			break;
		}
		case 0x10:
		case 0x51:
		case 0x45:
		case 0x11:
		{
			// Shift, Q, Ctrl, E키 뗌
			// blueball 상하움직임 취소
			CBlueBall* moveTarget = &g_target_blueball;
			moveTarget->setPower(moveTarget->getVelocity_X(), 0, moveTarget->getVelocity_Z());
			break;
		}
		case VK_UP:
		case VK_DOWN:
		{
			// 키보드 위, 아래측 버튼 뗌
			CBlueBall* moveTarget = &g_target_blueball;
			moveTarget->setPower(moveTarget->getVelocity_X(), moveTarget->getVelocity_Y(), 0);
			break;
		}
		case VK_LEFT:
		case VK_RIGHT:
		{
			// 키보드 좌, 우측 버튼 뗌
			CBlueBall* moveTarget = &g_target_blueball;
			moveTarget->setPower(0, moveTarget->getVelocity_Y(), moveTarget->getVelocity_Z());
			break;
		}
		}
		break;
	}

	case WM_MOUSEMOVE:
		//이거 카메라의 회전 기점이 광원으로 설정된거 같음
	{
		int new_x = LOWORD(lParam);
		int new_y = HIWORD(lParam);
		float dx;
		float dy;

		if (LOWORD(wParam) & MK_LBUTTON) {
			// 좌클릭
			// 화면 이동
			if (isReset) {
				isReset = false;
			}
			else {
				D3DXVECTOR3 vDist;
				D3DXVECTOR3 vTrans;
				D3DXMATRIX mTrans;
				D3DXMATRIX mX;
				D3DXMATRIX mY;

				switch (move) {
				case WORLD_MOVE:
					dx = (old_x - new_x) * 0.01f;
					dy = (old_y - new_y) * 0.01f;
					D3DXMatrixRotationY(&mX, dx);
					D3DXMatrixRotationX(&mY, dy);
					g_mWorld = g_mWorld * mX * mY;

					break;
				}
			}

			old_x = new_x;
			old_y = new_y;

		}
		else {
			isReset = true;
			// 우클릭
			// blue ball 움직이기
			if (LOWORD(wParam) & MK_RBUTTON) {
				if (isOriginTank) {
					dx = (old_x - new_x);// * 0.01f;
					dy = (old_y - new_y);// * 0.01f;
				}
				else {
					dx = -(old_x - new_x);// * 0.01f;
					dy = -(old_y - new_y);// * 0.01f;
				}

				D3DXVECTOR3 coord3d = g_target_blueball.getCenter();
				double nx = coord3d.x + dx * (-0.007f);
				double ny = coord3d.y;
				double nz = coord3d.z + dy * (0.007f);
				Tank* curTank = &tank;
				if (fabs(curTank->getCenter().x - nx) > MAX_BLUEBALL_WIDTH) {
					nx = coord3d.x;
				}
				if (fabs(curTank->getCenter().z - nz) < MIN_BLUEBALL_RADIUS || fabs(curTank->getCenter().z - nz) > MAX_BLUEBALL_RADIUS) {
					nz = coord3d.z;
				}
				g_target_blueball.setCenter(nx, ny, nz);
			}
			old_x = new_x;
			old_y = new_y;

			move = WORLD_MOVE;
		}
		break;
	}
	}

	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hinstance,
	HINSTANCE prevInstance,
	PSTR cmdLine,
	int showCmd)
{
	srand(static_cast<unsigned int>(time(NULL)));

	if (!d3d::InitD3D(hinstance,
		Width, Height, true, D3DDEVTYPE_HAL, &Device))
	{
		::MessageBox(0, "InitD3D() - FAILED", 0, 0);
		return 0;
	}

	if (!Setup())
	{
		::MessageBox(0, "Setup() - FAILED", 0, 0);
		return 0;
	}

	d3d::EnterMsgLoop(Display);

	Cleanup();

	Device->Release();

	return 0;
}