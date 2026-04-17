# Introduction and Project Goal
The project is "SpaceGlowTD" - a Tower Defense game set in a full 3D environment, written in C++ using Modern OpenGL. This is an academic project with rigorous technical requirements.
Current state: Successfully implemented procedural voxel map generation (using noise), continuous path generation on the heightmap, basic enemy logic (following the path), and an RTS-style camera setup.

# Technology Stack (Strictly Enforced)
- Language: C++ (C++17).
- Build System: CMake.
- Window & Context: GLFW + GLAD.
- Mathematics: GLM (mandatory for all transformations and vector math).
- External Tools: `tinyobjloader` (3D models), `stb_image` (textures), `Dear ImGui` (UI), `FastNoiseLite` (noise generation).
- FORBIDDEN: Using ready-made commercial engines (Unity, Unreal) or external rendering wrapper libraries.

# Hard OpenGL Rules (Failing these means failing the course)
1. No Deprecated OpenGL: Categorical ban on the Fixed-Function Pipeline.
   - FORBIDDEN functions include (but are not limited to): `glBegin`, `glEnd`, `glVertex`, `glNormal`, `glTexCoord`, `glRotate`, `glTranslate`, `glScale`, `glPushMatrix`, `glPopMatrix`, `glCallList`.
2. Modern Render Pipeline: All rendering must be done using VBOs, VAOs, index buffers, and `glDrawArrays` / `glDrawElements` calls. Transformations (Model, View, Projection) must be calculated by GLM and sent to shaders via Uniforms.
3. Zero Pink: Absolute ban on using the color pink (`#FF00FF`, `rgb(255, 0, 255)`) in any rendered element (textures, lights, clear color).
4. Lighting: The scene must have visible lighting. Every fired projectile must act as a dynamic Point Light source. Fragment shaders must support an array of point lights.

# Tower & Combat Mechanics (To be implemented)
Game logic relies on a 3D tile grid. Towers ignore other towers when checking for collision or line of sight.

1. Basic Tower (Homing Projectiles):
   - Fires a glowing projectile.
   - Every frame, the projectile recalculates its direction vector towards the target's current position.
   - Projectile is destroyed upon spherical collision with the enemy OR if its Y height drops below the Y height of the terrain tile it is currently flying over.

2. Mortar (Parabolic Trajectory):
   - Fires a projectile in an arc, simulating gravity.
   - Ignores terrain obstacles along the line of sight.
   - Upon hitting the ground (projectile Y <= terrain Y) or an enemy, it deals splash damage to all targets within a specific radius.

3. Laser (Continuous Beam):
   - Generates a continuous beam (e.g., scaled cylinder) dealing damage over time.
   - Requires constant Line of Sight (LoS) verification using "2.5D Raycasting" (linear interpolation of the beam's height over the XZ tile grid to check if terrain intersects the beam).
   - If terrain blocks the target, the laser visually hits the obstacle (beam ends at the height of the blocking tile) and deals no damage. This forces the tower to acquire a new target.

# Instructions for the AI Assistant (Code Generation Rules)
1. Clean & Simple Code: Write code as simple as possible in C++17. Avoid over-engineering, complex design patterns, or ECS unless absolutely necessary.
2. Short Comments: Instead of writing long essays explaining the code in the chat UI, put brief, concise comments directly inside the code snippets near key logic or specific OpenGL calls.
3. Step-by-Step: Solve only the specific problem the user is currently asking about. Provide ready-to-paste snippets or specific functions.
4. Technology Audit: Always double-check that the generated code contains zero deprecated OpenGL functions and strictly complies with the restrictions listed above.