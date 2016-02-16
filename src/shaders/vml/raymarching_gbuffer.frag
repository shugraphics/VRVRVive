#version 330

#define SAMPLES 2048
#define SAMPLES_RCP 0.0078125
#define PIXEL_SIZE 8
#define PI_RCP 0.31830988618379067153776752674503
#define BIAS 0.00005
#define PHI 10000000.0
#define TAU 0.0001

in vec2 passUV;

// sampler
uniform sampler2D noiseMap;
uniform sampler2D worldPosMap;
uniform sampler2D shadowMapSampler;

// light
uniform vec4 lightColor;
uniform mat4 lightView;
uniform mat4 lightProjection;

// camera
uniform vec4 cameraPosLightSpace;
uniform vec4 cameraViewDirInvLightSpace;

// raymarching
uniform float albedo;

// visibility function
float v(vec3 rayPositionLightSpace) {
    vec4 rayCoordLightSpace = lightProjection * vec4(rayPositionLightSpace, 1.0);
    rayCoordLightSpace.xyz /= rayCoordLightSpace.w;
    rayCoordLightSpace.xyz /= 2;
    rayCoordLightSpace.xyz += 0.5;

    float v = 1;
    if (texture(shadowMapSampler, rayCoordLightSpace.xy).z < rayCoordLightSpace.z - BIAS) {
        v = 0;
    }
    return v;
}

// phase function for anisotropic scattering
float p() {
    return 1.0f;
}

// calculate the lightintensity of the current sample at position rayPositionLightSpace
float executeRaymarching(vec3 x, float dl, float l) {
    // fetch whether the current position on the ray is visible form the light's perspective or not
    float v = v(x);
    // get distance of current ray postition to the light source in light view-space
    float d = length(x);
    float dRcp = 1.0/d;

    // calculate anisotropic scattering
    float p = p();

    // calculate the final light contribution for the sample on the way
    float radiantFluxAttenuation = PHI * PI_RCP * 0.25 * dRcp * dRcp;
    float Li = TAU * albedo * radiantFluxAttenuation * v * p;
    float intens = Li * exp(-TAU * d) * exp(-TAU * l) * dl;
    return intens;
}

/*void main() {

    float pixelBlockSize = PIXEL_SIZE * PIXEL_SIZE;

    // get index for the fragment within the pixelblock
    uint index = texture(noiseMap, passUV).x;

    // calculate number of samples
    float totalSampleNum = SAMPLES;
    float sampleNum = SAMPLES / pixelBlockSize;

    // calculate complete raymarching distance
    vec4 startPosLightSpace = lightView * texture(worldPosMap, passUV);
    float raymarchDistance = length(cameraPosLightSpace.xyz - startPosLightSpace.xyz);
    float stepSize = raymarchDistance / totalSampleNum;
    float stepSizePixel = raymarchDistance / sampleNum;

    // calculate the stepsize of the ray
    vec3 viewDir = normalize(cameraPosLightSpace.xyz - startPosLightSpace.xyz);
    vec3 rayDir = viewDir * stepSize * pixelBlockSize;

    // offset starting position
    vec3 offset = viewDir  * index * stepSize;
    vec3 rayPositionLightSpace = startPosLightSpace.xyz + offset;

    // total light contribution accumulated along the ray
    float vli = 0.0f;
    for (float i = 0; i <sampleNum-1; i++) {
        vec3 currentPos = rayPositionLightSpace + i * rayDir;
        float distanceToCamera = length(cameraPosLightSpace.xyz - currentPos.xyz);
        vli += executeRaymarching(currentPos, stepSize, distanceToCamera);
    }

    vli /= sampleNum;

    gl_FragColor =  vec4(lightColor.xyz * vli, 1.0);

}*/

void main() {

    int blockSize = PIXEL_SIZE * PIXEL_SIZE;

    // get index for the fragment within the pixelblock
    int index = int(texture(noiseMap, passUV).r * 255.0);

    // calculate number of samples
    float totalSampleNum = SAMPLES;
    float sampleNum = SAMPLES / blockSize;

    // calculate complete raymarching distance
    vec4 startPosLightSpace = lightView * texture(worldPosMap, passUV);
    vec3 invViewDir = cameraPosLightSpace.xyz - startPosLightSpace.xyz;
    float s = length(invViewDir);
    float dl = s / totalSampleNum;
    float dlb = blockSize * dl;

    // calculate the stepsize of the ray
    vec3 viewDirNormal = normalize(invViewDir);
    vec3 viewDir = viewDirNormal * dlb;

    // offset starting position
    vec3 offset = viewDirNormal * index * dl;
    float o = length(offset);
    vec3 x = startPosLightSpace.xyz + offset;

    // total light contribution accumulated along the ray
    float vli = 0.0f;
    for (float l = s - o - dl; l >= 0;  l -= dlb) {
        x +=  viewDir;
        vli += executeRaymarching(x, dl, l);
    }
    vli /= sampleNum;

    gl_FragColor =  vec4(lightColor.xyz * vli, 1);
/*    if (index <= 16)  gl_FragColor = vec4(1, 0, 0, 1);
    if (index > 16)  gl_FragColor = vec4(0, 1, 0, 1);
    if (index > 32)  gl_FragColor = vec4(0, 0, 1, 1);
    if (index > 48)  gl_FragColor = vec4(1, 0, 1, 1);
    if (index > 1000)  gl_FragColor = vec4(1, 1, 0, 1);*/
}
