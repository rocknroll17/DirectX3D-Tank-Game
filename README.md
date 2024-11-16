# DirectX3D Tank Game

A 3D tank game built using DirectX 3D, showcasing a fully interactive tank battle experience with customizable graphics settings and real-time gameplay features.
![image](https://github.com/user-attachments/assets/217c6b4e-c083-4fd3-aa33-72f169ac3746)
Tank
![20231118_213532](https://github.com/user-attachments/assets/c76cea3c-8967-4ae3-a29b-e16bc6df4728)


## Table of Contents
- [Project Overview](#project-overview)
- [Features](#features)
- [Technologies Used](#technologies-used)
- [Installation](#installation)
- [Usage](#usage)
- [Contributors](#Contributors)

## Project Overview
The DirectX3D Tank Game is a 3D action game where players control a tank on a battlefield and engage in dynamic combat with enemy tanks. The project demonstrates essential 3D game development skills, including real-time rendering, collision detection, physics handling, and user interaction, utilizing DirectX 3D.

This game was designed as a team project for college assigment with DirectX 3D & OOP.

## System Requirements
Develop environment: Microsoft Visual Studio Community 2022 (64-bit)
Graphic API: DirectX 12

## Features
- **Tank Controls**: Smooth and responsive controls for navigating and aiming.
- **Real-Time Rendering**: Real-Time graphics rendering using DirectX 3D.
- **Collision Detection**: Accurate collision handling between tanks, projectiles and obstacles
- **Dual Mode**: Two players can take turns to play.
- **Perspective**: The perspective includes first-person, third-person, and overhead views, with the first-person perspective particularly resembling the view from within the tank itself.
- **Camera Movement**: The camera follows the projectile, creating a dynamic camera view.

## Technologies Used
- **DirectX 3D**: For graphics rendering and creating the 3D environment.
- **C++**: Primary programming language.
  
## Installation
To set up and run the game locally, follow these steps:

1. **Clone the repository**:
    ```bash
    git clone https://github.com/rocknroll17/DirectX3D-Tank-Game.git
    ```

2. **Open the Solution File**:
   - Open `VirtualLego.sln` in Visual Studio. This solution includes `d3dUtility.cpp`, `virtualLego.cpp`, and `d3dUtility.h`.

3. **Set Up Project Properties**:
   - Go to the **Property** tab of the project.

4. **Edit VC++ Directories**:
   - Under **VC++ Directories**, set the **Include Directories** to `{SDK installation path}\Include`.
   - Set the **Library Directories** to `{SDK installation path}\Lib\x86`.

5. **Enable Function-Level Linking**:
   - Go to **Code Generation** and set **Enable Function-Level Linking** to `Yes (/Gy)`.

6. **Run the Project**:
   - Press `Ctrl + F5` to execute the program.

## Usage

### Controls
- **Tank Movement**:
  - `W`: Move the tank forward
  - `A`: Move the tank left
  - `S`: Move the tank backward
  - `D`: Move the tank right

- **Target Point (Blue Ball) Movement**:
  - `Ctrl` or `E`: Move the target point down
  - `Shift` or `Q`: Move the target point up
  - `↑` or **Right Mouse Button**: Move the target point forward
  - `←` or **Right Mouse Button**: Move the target point left
  - `↓` or **Right Mouse Button**: Move the target point backward
  - `→` or **Right Mouse Button**: Move the target point right

- **Actions**:
  - `Space`: Fire a shell & skip the start screen
  - `Enter`: Toggle rendering state & skip the start screen
  - `V`, `C`, `1 ~ 9`: Switch camera view options

## Contributors
<a href="https://github.com/rocknroll17">
  <img src="https://github.com/rocknroll17.png" width="50" height="50" alt="rocknroll17">
</a>
<br>
<a href="https://github.com/ja7811">
  <img src="https://github.com/ja7811.png" width="50" height="50" alt="ja7811">
</a>
<br>
<a href="https://github.com/dkTkrkdhfl">
  <img src="https://github.com/dkTkrkdhfl.png" width="50" height="50" alt="dkTkrkdhfl">
</a>
<br>
<a href="https://github.com/Hyun0828">
  <img src="https://github.com/Hyun0828.png" width="50" height="50" alt="Hyun0828">
</a>
