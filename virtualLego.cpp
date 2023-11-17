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

using namespace std;

IDirect3DDevice9* Device = NULL;

// window size
const int Width = 1920;
const int Height = 1080;
const double TANK_SPEED = 0.45;

// There are four balls
// initialize the position (coordinate) of each ball (ball0 ~ ball3)
const float spherePos[4][2] = { {-2.7f,0} , {+2.4f,0} , {3.3f,0} , {-2.7f,-0.9f} };
// initialize the color of each ball (ball0 ~ ball3)
const D3DXCOLOR sphereColor[4] = { d3d::RED, d3d::RED, d3d::YELLOW, d3d::WHITE };

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

#define BLUEBALL_MOVE_DISTANCE 0.07
#define MAX_BLUEBALL_RADIUS 2  // blueball 어디까지 멀어질 수 있는지
#define MIN_BLUEBALL_RADIUS 0.2 // blueball 어디 이상 멀어져야 하는지 (앞으로)
#define MAX_BLUEBALL_WIDTH 0.3 // blueball 어디까지 멀어질 수 있는지 (옆으로)

#define MISSILE_POWER 1.88
#define MISSILE_GRAVITY_RATE 0.6
#define MISSILE_DECREASE_RATE 0.9982
#define MISSILE_EXPOLSION_RADIUS M_RADIUS+0.25 // 미사일 폭발 반경

#define WORLD_WIDTH 24
#define WORLD_DEPTH 36
#define BORDER_WIDTH 0.12f // 가장자리 벽 굵기

#define NUM_OBSTACLE 10	// 장애물 개수

bool GAME_START = false;
float MOVEMENT = 0.0f;

double g_camera_pos[3] = { 0.0, 5.0, -8.0 };
bool camera_option = 0;
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


		//correction of position of ball
		// Please uncomment this part because this correction of ball position is necessary when a ball collides with a wall
		/*
		if (tX >= ((WORLD_WIDTH / 2) - M_RADIUS))
			tX = (WORLD_WIDTH / 2) - M_RADIUS;
		else if (tX <= (-(WORLD_WIDTH / 2) + M_RADIUS))
			tX = -(WORLD_WIDTH / 2) + M_RADIUS;
		else if (tZ <= (-(WORLD_DEPTH / 2) + M_RADIUS))
			tZ = -(WORLD_DEPTH / 2) + M_RADIUS;
		else if (tZ >= ((WORLD_DEPTH / 2) - M_RADIUS))
			tZ = (WORLD_DEPTH / 2) - M_RADIUS;
		*/
		if (tY < 0 + M_RADIUS)
			tY = M_RADIUS;
		// y가 0 이하로 떨어지지 않도록 (임시)

		this->setCenter(tX, tY, tZ);
		Out();	// 미사일이 밖으로 나가면 바닥에 닿을 때 터짐

		//this->setPower(this->getVelocity_X() * DECREASE_RATE, this->getVelocity_Z() * DECREASE_RATE);
		double rate = 1 - (1 - DECREASE_RATE) * timeDiff * 400;
		if (rate < 0)
			rate = 0;
		this->setPower(getVelocity_X() * rate, getVelocity_Y() - 12 * timeDiff, getVelocity_Z() * rate);

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

		D3DXVECTOR3 wallCenter = wall.getCenter();
		float wallWidth = wall.getWidth();
		float wallDepth = wall.getDepth();

		bool intersectX = (wallCenter.x - wallWidth / 2 <= Center.x + width / 2) && (wallCenter.x + wallWidth / 2 >= Center.x - width / 2);
		bool intersectZ = (wallCenter.z - wallDepth / 2 <= Center.z + depth / 2) && (wallCenter.z + wallDepth / 2 >= Center.z - depth / 2);

		return intersectX && intersectZ;
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
	CWall tank_part[3];
	bool created;
public:
	Tank(bool isOtank) {
		m_velocity_x = 0;
		m_velocity_z = 0;
		isO = isOtank;
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
		return tank_part[0].hasIntersected(missile) || tank_part[1].hasIntersected(missile) || tank_part[2].hasIntersected(missile);
	}

	bool hasIntersected(CObstacle& obstacle)
	{
		return tank_part[0].hasIntersected(obstacle);
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


		//this->setPower(this->getVelocity_X() * DECREASE_RATE, this->getVelocity_Z() * DECREASE_RATE);
		double rate = 1 - (1 - TANK_VELOCITY_RATE) * timeDiff * 400;
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

		// 탱크가 움직이면 blueball도 움직임

		double dX = tankX - tankLastX;
		double dZ = tankZ - tankLastZ;
		tX += dX;
		tZ += dZ;
		this->setCenter(tX, tY, tZ);
		double rate = 1 - (1 - DECREASE_RATE) * timeDiff * 400;
		if (rate < 0)
			rate = 0;
		this->setPower(getVelocity_X() * rate, getVelocity_Y() * rate, getVelocity_Z() * rate);

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
Tank tank(0);
Tank otank(1);

CObstacle obstacle; // 장애물 (테스트용)
CObstacle obstacle1;
vector<CObstacle> obstacles;	// 랜덤 장애물 모음
vector<CObstacle> obstacle_wall; // 장애물 (벽)

LPD3DXFONT fonts; // test -> 화면에 숫자표시 이걸로 하는듯

CSphere missile;   // c 누르면 나가는 미사일

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

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


bool createWall(float partitionWidth, float partitionHeight, float partitonDepth,
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

bool createObstacle() // create obstacle
{
	// 장애물 랜덤 생성
	for (int i = 0; i < NUM_OBSTACLE; i++) {
		random_device rd;
		mt19937 gen(rd());

		uniform_real_distribution<float> disW(0, 1.0f);	// width<=1.0f
		uniform_real_distribution<float> disD(0, 1.5f);	// depth<=1.5f
		uniform_real_distribution<float> disH(0, 2.5f);	// height<=2.5f

		float width = disW(gen);
		float depth = disD(gen);
		float height = disH(gen);

		uniform_real_distribution<float> disX(-(WORLD_WIDTH / 2) + width / 2, (WORLD_WIDTH / 2) - width);	// x 범위
		uniform_real_distribution<float> disZ(-(WORLD_WIDTH / 2) + depth / 2, (WORLD_WIDTH / 2) - depth);	// z 범위

		float x = disX(gen);
		float y = height / 2;
		float z = disZ(gen);

		CObstacle obs;
		if (false == obs.create(Device, x, z, width, height, depth, d3d::WHITE)) return false;
		obs.setPosition(x, y, z);

		obstacles.push_back(obs);
	}
	return true;
}

bool createMap() // create plane + wall
{
	CWall wall;

	if (false == g_legoPlane.create(Device, -1, -1, WORLD_WIDTH, 0.03f, WORLD_DEPTH, d3d::GREEN)) return false;
	g_legoPlane.setPosition(0.0f, -0.0006f / 5, 0.0f);

	if (false == wall.create(Device, -1, -1, 11.0f, 2.0f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(-6.0f, 1.0f, -18.0f);
	lwall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 11.0f, 2.0f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(6.0f, 1.0f, -18.0f);
	lwall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 11.0f, 2.0f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(6.0f, 1.0f, 18.0f);
	lwall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 11.0f, 2.0f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(-6.0f, 1.0f, 18.0f);
	lwall1.push_back(wall);

	g_legoWall.push_back(lwall1);

	if (false == wall.create(Device, -1, -1, 1.0f, 3.0f, 1.5f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.5f, -18.0f);
	swall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.5f, 1.5f, d3d::GRAY)) return false;
	wall.setPosition(0, 1.25f, -18.0f);
	swall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 3.0f, 1.5f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.5f, -18.0f);
	swall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 3.0f, 1.5f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.5f, 18.0f);
	swall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.5f, 1.5f, d3d::GRAY)) return false;
	wall.setPosition(0, 1.25f, 18.0f);
	swall1.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 3.0f, 1.5f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.5f, 18.0f);
	swall1.push_back(wall);

	g_legoWall.push_back(swall1);

	if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, 11.0f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.0f, 12.0f);
	lwall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, 11.0f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.0f, 0);
	lwall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, 11.0f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.0f, -12.0f);
	lwall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, 11.0f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.0f, 12.0f);
	lwall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, 11.0f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.0f, 0);
	lwall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.0f, 2.0f, 11.0f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.0f, -12.0f);
	lwall2.push_back(wall);

	g_legoWall.push_back(lwall2);

	if (false == wall.create(Device, -1, -1, 1.5f, 2.5f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.25f, 6.0f);
	swall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.5f, 2.5f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(-12.0f, 1.25f, -6.0f);
	swall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.5f, 2.5f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.25f, 6.0f);
	swall2.push_back(wall);
	if (false == wall.create(Device, -1, -1, 1.5f, 2.5f, 1.0f, d3d::GRAY)) return false;
	wall.setPosition(12.0f, 1.25f, -6.0f);
	swall2.push_back(wall);

	g_legoWall.push_back(swall2);

	return true;
}

void destroyAllLegoBlock(void)
{
}

// initialization
bool Setup()
{
	int i;

	D3DXMatrixIdentity(&g_mWorld);
	D3DXMatrixIdentity(&g_mView);
	D3DXMatrixIdentity(&g_mProj);

	if (false == tank.create(Device, -1, -1, d3d::BROWN)) return false;
	tank.setPosition(-1, 0.2f, -1);

	if (false == otank.create(Device, -1, -1, d3d::BROWN)) return false;
	otank.setPosition(-1, 0.2f, 1);

	// tank랑 blue ball 연결
	g_target_blueball.linkTank(&tank);

	// 벽, 바닥 생성
	createMap();
	// 장애물 생성
	createObstacle();

	// 장애물(벽) 생성
	// 벽 하나는 여러개의 파티션으로 나누어짐
	D3DXCOLOR wall_color = d3d::BLUE; // 벽 색상
	float wallPartition_width = 0.12f; // 각 파티션의 가로넓이
	float wallPartition_height = 0.6f; // 각 파티션의 높이
	float wallPartition_depth = 1; // 각 파티션의 세로넓이
	int partitionCount_land = 3; // 가로로 몇 개 놓을지
	int partitionCount_sky = 3; // 세로로 몇 개 놓을지
	float base_x = 0.0f, base_y = wallPartition_height * 0.5, base_z = -3.0f; // 벽 생성 위치

	// 벽 생성 & 배치
	createWall(wallPartition_width, wallPartition_height, wallPartition_depth, partitionCount_land, partitionCount_sky, base_x, base_y, base_z, wall_color);

	// 블럭 생성 (테스트)
	createBlock(0.2f, 0.2f, 0.2f, 10, 10, 10, 2, 0, 2, d3d::BROWN);

	// create blue ball for set direction
	if (false == g_target_blueball.create(Device, d3d::BLUE)) return false;
	g_target_blueball.setCenter(.0f, (float)M_RADIUS + 3, .0f);

	// light setting 
	D3DLIGHT9 lit;
	::ZeroMemory(&lit, sizeof(lit));
	lit.Type = D3DLIGHT_POINT;
	lit.Diffuse = d3d::WHITE * 1.8f;  // 원래 1배였음
	lit.Specular = d3d::WHITE * 1.5f;  //원래 0.9배였음
	lit.Ambient = d3d::WHITE * 0.9f;//원래 0.9배였음
	lit.Position = D3DXVECTOR3(0.0f, 4.0f, 5.0f);
	lit.Range = 100.0f;
	lit.Attenuation0 = 0.0f;//상수 감쇠
	lit.Attenuation1 = 0.3f;//선형감쇠 원래 0.9f였음.
	lit.Attenuation2 = 0.0f;//제곱감쇠
	if (false == g_light.create(Device, lit))
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

}

float x_camera = 0.0f;
float y_camera = 0.8f;

float back_camera = 1;
float camera_prefix = 0.05f;

bool isOriginTank = TRUE;

float lastBlueballCenterx = 0.0f;
float lastBlueballCentery = (float)M_RADIUS + 1;
float lastBlueballCenterz = -3.0f;

// timeDelta represents the time between the current image frame and the last image frame.
// the distance of moving balls should be "velocity * timeDelta"
bool Display(float timeDelta)
{
	int i = 0;
	int j = 0;
	D3DXVECTOR3 pos;
	D3DXVECTOR3 target;
	D3DXVECTOR3 up;


	if (GAME_START == false) {
		pos = D3DXVECTOR3(20.0f, 12.0f, -WORLD_DEPTH / 2 + MOVEMENT);
		target = D3DXVECTOR3(0.0f, 0.0f, -WORLD_DEPTH / 2 + MOVEMENT);

		MOVEMENT = MOVEMENT + 0.001;
		if (MOVEMENT > WORLD_DEPTH) {
			GAME_START = true;
		}
		D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
		Device->SetTransform(D3DTS_VIEW, &g_mView);
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
		else {
			pos = D3DXVECTOR3(20.0, 10.0, 0.0);
			target = D3DXVECTOR3(0, 1, 0);
		}
	}

	up = D3DXVECTOR3(0.0f, 2.0f, 0.0f);
	D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
	Device->SetTransform(D3DTS_VIEW, &g_mView);

	static double startTime = (double)timeGetTime();

	if (Device)
	{
		double currTime = (double)timeGetTime();
		double timediff = currTime - startTime;

		if (timediff > 10000) {
			Tank tempTank = tank;
			tank = otank;
			otank = tempTank;
			g_target_blueball.linkTank(&tank);

			startTime = currTime;
			float tempFloatx, tempFloaty, tempFloatz;

			tempFloatx = lastBlueballCenterx;
			lastBlueballCenterx = g_target_blueball.getCenter().x;
			tempFloaty = lastBlueballCentery;
			lastBlueballCentery = g_target_blueball.getCenter().y;
			tempFloatz = lastBlueballCenterz;
			lastBlueballCenterz = g_target_blueball.getCenter().z;
			g_target_blueball.setCenter(tempFloatx, tempFloaty, tempFloatz);

			if (isOriginTank) {
				isOriginTank = FALSE;
			}
			else {
				isOriginTank = TRUE;
			}
		}


		Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00afafaf, 1.0f, 0);
		Device->BeginScene();
		// 탱크 위치 변경
		tank.tankUpdate(timeDelta, obstacles, otank, g_legoWall);
		// 미사일 위치도 변경 & 벽과 충돌했는지 체크
		missile.ballUpdate(timeDelta);
		for (i = 0; i < g_legoWall.size(); i++) {
			for (j = 0; j < g_legoWall[i].size(); j++)
				g_legoWall[i][j].hitBy(missile);
		}

		// 블루볼 위치 변경
		g_target_blueball.ballUpdate(timeDelta);;
		// check whether any two balls hit together and update the direction of balls

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
				exit(1);		// 탱크 터지면 메시지 띄우고 바로 끝내야 할 듯, 안 끝내면 otank 이미 사라져서 오류뜸
			}
			else if (otank.get_created()) {
				otank.tankUpdate(timeDelta, obstacles, tank, g_legoWall);
				otank.draw(Device, g_mWorld);
			}
		}

		// 랜덤 장애물 파괴 체크 & 파괴 안되면 그림
		for (int i = 0; i < obstacles.size(); i++) {
			if (obstacles[i].get_created()) {
				if (obstacles[i].hasIntersected(missile)) {
					obstacles[i].hitBy(missile);
					// 만약 장애물 파괴시, 폭발한다 (= 주변 장애물도 다시 그림)
					// 스파게티 코드지만 마감 얼마 안남았으니까 이대로 갑시다
					double mx = missile.getCenter().x;
					double my = missile.getCenter().y;
					double mz = missile.getCenter().z;
					for(int j = 0; j<obstacles.size(); j++){ 
						if (obstacles[j].hasIntersected(mx, my, mz, MISSILE_EXPOLSION_RADIUS)) {
							obstacles[j].hitBy(missile);
						}
					}
					for(int j = 0; j < obstacle_wall.size(); j++){ 
						if (obstacle_wall[j].hasIntersected(mx, my, mz, MISSILE_EXPOLSION_RADIUS)) {
							obstacle_wall[j].hitBy(missile);
						}
					}
				}
				else {
					obstacles[i].draw(Device, g_mWorld);
				}
			}
		}

		// 장애물(벽) 파괴 체크 & 파괴 안되면 그림
		for (int i = 0; i < obstacle_wall.size(); i++) {
			if (obstacle_wall[i].get_created()) {
				if (obstacle_wall[i].hasIntersected(missile)) {
					obstacle_wall[i].hitBy(missile);
					// 만약 장애물 파괴시, 폭발한다 (= 주변 장애물도 다시 그림)
					// 스파게티 코드지만 마감 얼마 안남았으니까 이대로 갑시다
					for (int j = 0; j < obstacles.size(); j++) {
						if (obstacles[j].hasIntersected(missile.getCenter().x, missile.getCenter().y, missile.getCenter().z, MISSILE_EXPOLSION_RADIUS)) {
							obstacles[j].hitBy(missile);
						}
					}
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

		g_light.draw(Device);

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
		{
			// 스페이스바 누름
			// 게임 시작 전일 경우, 게임 시작하고 끝
			if (!GAME_START) {
				GAME_START = true;
				break;
			}
			// 파란 공 쪽으로 미사일 발사
			if (!missile.getCreated()) {
				D3DXVECTOR3 targetpos = g_target_blueball.getCenter();
				D3DXVECTOR3	whitepos = tank.getHead();
				double theta = acos(
					sqrt(pow(targetpos.x - whitepos.x, 2)) /
					sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2))
				);		// 기본 1 사분면
				if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x >= 0) { theta = -theta; }	//4 사분면
				if (targetpos.z - whitepos.z >= 0 && targetpos.x - whitepos.x <= 0) { theta = PI - theta; } //2 사분면
				if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x <= 0) { theta = PI + theta; } // 3 사분면
				double distance_land = sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2)); // xz만 고려한 거리

				double theta_sky = acos(
					sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2)) /
					sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.y - whitepos.y, 2) + pow(targetpos.z - whitepos.z, 2))
				);
				double distance_sky = sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.y - whitepos.y, 2) + pow(targetpos.z - whitepos.z, 2));  // y좌표 고려한 거리
				//double distance = sqrt( sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2)) + pow(targetpos.y - whitepos.y, 2)); // y좌표 포함 계산

				missile.destroy();
				missile.create(Device, d3d::BLACK);
				missile.setCenter(whitepos.x, whitepos.y, whitepos.z);
				missile.setPower(distance_land * cos(theta) * MISSILE_POWER, distance_sky * sin(theta_sky), distance_land * sin(theta) * MISSILE_POWER);
			}
			break;
		}

		case VK_LEFT:
		{
			// 키보드 좌측 버튼
			Tank* moveTarget = &tank;  // 움직일 대상
			double speed = TANK_SPEED;
			if (isOriginTank) {
				moveTarget->setPower(-(speed * 5), 0);
			}
			else {
				moveTarget->setPower(speed * 5, 0);
			}
			break;
		}

		case VK_RIGHT:
		{
			// 키보드 우측 버튼
			Tank* moveTarget = &tank;;  // 움직일 대상
			double speed = TANK_SPEED;
			if (isOriginTank) {
				moveTarget->setPower(speed * 5, 0);
			}
			else {
				moveTarget->setPower(-(speed * 5), 0);
			}
			break;
		}

		case VK_UP:
		{
			// 키보드 위측 버튼
			Tank* moveTarget = &tank;  // 움직일 대상
			double speed = TANK_SPEED;
			if (isOriginTank) {
				moveTarget->setPower(0, speed * 5);
			}
			else {
				moveTarget->setPower(0, -(speed * 5));
			}
			break;
		}

		case VK_DOWN:
		{
			// 키보드 아래측 버튼
			Tank* moveTarget = &tank;;  // 움직일 대상
			double speed = TANK_SPEED;
			if (isOriginTank) {
				moveTarget->setPower(0, -(speed * 5));
			}
			else {
				moveTarget->setPower(0, speed * 5);
			}
			break;
		}
		/* 수곤이버전
		case 0x57:
		{
			// W키
			CBlueBall* moveTarget = &g_target_blueball;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 v = moveTarget->getCenter();
			//double newZ = v.z + distance;
			moveTarget->setCenter(v.x, v.y, v.z + distance);
			break;
		}

		case 0x41:
		{
			// A키
			CBlueBall* moveTarget = &g_target_blueball;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 v = moveTarget->getCenter();
			moveTarget->setCenter(v.x - distance, v.y, v.z);
			break;
		}

		case 0x53:
		{
			// S키
			CBlueBall* moveTarget = &g_target_blueball;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 v = moveTarget->getCenter();
			moveTarget->setCenter(v.x, v.y, v.z - distance);
			break;
		}
		case 0x44:
		{
			// D키
			CBlueBall* moveTarget = &g_target_blueball;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 v = moveTarget->getCenter();
			moveTarget->setCenter(v.x + distance, v.y, v.z);
			break;
		}
		*/
		// 재환이 + 나 버전
		case 0x57:
		{
			// W키
			CBlueBall* moveTarget = &g_target_blueball;
			Tank* shooter = &tank;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 tankcoord = shooter->getCenter();
			D3DXVECTOR3 targetcoord = moveTarget->getCenter();
			double dz = targetcoord.z - tankcoord.z;
			if (isOriginTank) {
				if (dz < MAX_BLUEBALL_RADIUS) moveTarget->setCenter(targetcoord.x, targetcoord.y, targetcoord.z + distance);
			}
			else {
				dz *= -1;
				if (dz < MAX_BLUEBALL_RADIUS) moveTarget->setCenter(targetcoord.x, targetcoord.y, targetcoord.z - distance);
			}
			break;
		}

		case 0x41:
		{
			// A키
			CBlueBall* moveTarget = &g_target_blueball;
			Tank* shooter = &tank;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 tankcoord = shooter->getCenter();
			D3DXVECTOR3 targetcoord = moveTarget->getCenter();
			double dx = tankcoord.x - targetcoord.x;
			if (isOriginTank) {
				if (dx < MAX_BLUEBALL_WIDTH) moveTarget->setCenter(targetcoord.x - distance, targetcoord.y, targetcoord.z);
			}
			else {
				dx *= -1;
				if (dx < MAX_BLUEBALL_WIDTH) moveTarget->setCenter(targetcoord.x + distance, targetcoord.y, targetcoord.z);
			}
			break;
		}

		case 0x53:
		{
			// S키
			CBlueBall* moveTarget = &g_target_blueball;
			Tank* shooter = &tank;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 tankcoord = shooter->getCenter();
			D3DXVECTOR3 targetcoord = moveTarget->getCenter();
			double dz = targetcoord.z - tankcoord.z;
			if (isOriginTank) {
				if (dz > MIN_BLUEBALL_RADIUS) moveTarget->setCenter(targetcoord.x, targetcoord.y, targetcoord.z - distance);
			}
			else {
				dz *= -1;
				if (dz > MIN_BLUEBALL_RADIUS) moveTarget->setCenter(targetcoord.x, targetcoord.y, targetcoord.z + distance);
			}
			break;
		}
		case 0x44:
		{
			// D키
			CBlueBall* moveTarget = &g_target_blueball;
			Tank* shooter = &tank;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 tankcoord = shooter->getCenter();
			D3DXVECTOR3 targetcoord = moveTarget->getCenter();
			double dx = targetcoord.x - tankcoord.x;

			if (isOriginTank) {
				if (dx < MAX_BLUEBALL_WIDTH) moveTarget->setCenter(targetcoord.x + distance, targetcoord.y, targetcoord.z);
			}
			else {
				dx *= -1;
				if (dx < MAX_BLUEBALL_WIDTH) moveTarget->setCenter(targetcoord.x - distance, targetcoord.y, targetcoord.z);
			}
			break;
		}
		case 0x56:
		{
			// v 버튼 누를 시
			if (camera_option == 0) { camera_option = 1; }
			else { camera_option = 0; }
			break;
		}
		case 0x10:
		case 0x51:
		{
			// Shift, Q키
			// blueball 올림
			CBlueBall* moveTarget = &g_target_blueball;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 v = moveTarget->getCenter();
			moveTarget->setCenter(v.x, v.y + distance, v.z);
			break;
		}
		case 0x45:
		case 0x11:
		{
			// Ctrl키
			// blueball 내림
			CBlueBall* moveTarget = &g_target_blueball;
			double distance = BLUEBALL_MOVE_DISTANCE;
			D3DXVECTOR3 v = moveTarget->getCenter();
			moveTarget->setCenter(v.x, v.y - distance, v.z);
			break;
		}

		case 0x43:
		{
			back_camera = back_camera * -1;
			break;
		}

		case 0x61:
		{
			x_camera = x_camera - camera_prefix;
			y_camera = y_camera - camera_prefix;
			break;
		}
		case 0x62:
		{
			y_camera = y_camera - camera_prefix;
			break;
		}
		case 0x63:
		{
			y_camera = y_camera - camera_prefix;
			x_camera = x_camera + camera_prefix;
			break;
		}
		case 0x64:
		{
			x_camera = x_camera - camera_prefix;
			break;
		}
		case 0x65:
		{
			x_camera = 0.0f;
			y_camera = 0.5f;
			break;
		}
		case 0x66:
		{
			x_camera = x_camera + camera_prefix;
			break;
		}
		case 0x67:
		{
			x_camera = x_camera - camera_prefix;
			y_camera = y_camera + camera_prefix;
			break;
		}
		case 0x68:
		{
			y_camera = y_camera + camera_prefix;
			break;
		}
		case 0x69:
		{
			x_camera = x_camera + camera_prefix;
			y_camera = y_camera + camera_prefix;
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
				dx = (old_x - new_x);// * 0.01f;
				dy = (old_y - new_y);// * 0.01f;

				D3DXVECTOR3 coord3d = g_target_blueball.getCenter();
				g_target_blueball.setCenter(coord3d.x + dx * (-0.007f), coord3d.y, coord3d.z + dy * 0.007f);
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