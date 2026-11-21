#version 330 core

in vec3 WorldNormal;
in vec2 TexCoords;
in float AO;

// Output
out vec4 FragColor;

// Texture sampler for the 256x256 atlas
uniform sampler2D texAtlas;

// Material
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float shininess = 32.0;

// Block type colors (approximate)
struct BlockColor {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

const BlockColor blockColors[16] = {
    [0] = {vec3(0.0), vec3(0.0), vec3(0.0)},       // Air - invisible
    [1] = {vec3(0.1), vec3(0.5, 0.5, 0.5), vec3(0.0)},   // Stone
    [2] = {vec3(0.1), vec3(0.6, 0.4, 0.2), vec3(0.0)},   // Dirt
    [3] = {vec3(0.1), vec3(0.1, 0.8, 0.1), vec3(0.0)},   // Grass
    [4] = {vec3(0.1), vec3(0.5, 0.3, 0.1), vec3(0.1)},   // Wood
    [5] = {vec3(0.1), vec3(0.2, 0.7, 0.2), vec3(0.0)},   // Leaves
    [6] = {vec3(0.1), vec3(0.8, 0.8, 0.2), vec3(0.0)},   // Sand
    [7] = {vec3(0.1), vec3(0.1, 0.5, 0.8), vec3(0.0)},   // Water
    [8] = {vec3(0.1), vec3(0.3, 0.3, 0.3), vec3(0.0)},   // Bedrock
    [9] = {vec3(0.1), vec3(0.7, 0.6, 0.4), vec3(0.1)},   // Planks
    [10] = {vec3(0.1), vec3(0.6, 0.6, 0.6), vec3(0.0)},  // Sticks
    [11] = {vec3(0.1), vec3(0.8, 0.6, 0.3), vec3(0.1)},  // CraftingTable
    [12] = {vec3(0.1), vec3(0.7, 0.3, 0.3), vec3(0.1)},  // Brick
    [13] = {vec3(0.0), vec3(0.0), vec3(0.0)},  // placeholder
    [14] = {vec3(0.0), vec3(0.0), vec3(0.0)},  // placeholder
    [15] = {vec3(0.0), vec3(0.0), vec3(0.0)}   // Count
};

void main()
{
    // Get block type from texture coordinate (first channel)
    vec4 texColor = texture(texAtlas, TexCoords);
    
    // Use the block type index from RGB
    int blockType = int(texColor.r * 15.0); // assuming type index 0-15 packed in r
    
    // Clamp to valid range
    if (blockType < 0) blockType = 0;
    if (blockType >= 16) blockType = 15;
    
    BlockColor bc = blockColors[blockType];
    
    // Ambient
    vec3 ambient = bc.ambient;
    
    // Diffuse lighting (Lambert)
    vec3 normal = normalize(WorldNormal);
    vec3 lightDir = normalize(lightPos - vec3(gl_FragCoord.xy, 0.0));
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = bc.diffuse * diff;
    
    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(viewPos - vec3(gl_FragCoord.xy, 0.0));
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = bc.specular * spec;
    
    // Combine
    vec3 result = ambient + diffuse + specular;
    
    // Apply AO
    result *= AO;
    
    // Add some water transparency effect
    if (blockType == 7) { // Water
        result *= vec3(0.5); // Dim water
        FragColor = vec4(result, 0.6); // Semi-transparent
    } else {
        FragColor = vec4(result, 1.0);
    }
}