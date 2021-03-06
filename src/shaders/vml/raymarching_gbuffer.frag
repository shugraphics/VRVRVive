#version 330

#define SAMPLES 2048
#define SAMPLES_RCP 0.0078125
#define PIXEL_SIZE 8
#define PI_RCP 0.31830988618379067153776752674503
#define BIAS 0.00005
#define PHI 10000000.0
#define TAU 0.0001
#define LIMIT 50.0

in vec2 passUV;

// sampler
uniform sampler2D noiseMap;
uniform sampler2D worldPosMap;
uniform sampler2D shadowMap;

// matrices
uniform mat4 viewToLight;
uniform mat4 lightProjection;
uniform vec4 cameraPosLightSpace;

// raymarching
uniform float phi;
uniform float tau;
uniform float albedo;
uniform float g;

uniform bool useALS;

// light color
uniform vec3 lightColor;

// clamping
uniform float clampMax;

// visibility function
float v(vec3 rayPositionLightSpace) {
    vec4 rayCoordLightSpace = lightProjection * vec4(rayPositionLightSpace, 1.0);
    rayCoordLightSpace.xyz /= rayCoordLightSpace.w;
    rayCoordLightSpace.xyz *= 0.5;
    rayCoordLightSpace.xyz += 0.5;

    float v = 1;
    if (any( lessThan(rayCoordLightSpace.xy, vec2(0.0,0.0))) || any ( greaterThan(rayCoordLightSpace.xy,vec2(1.0,1.0) )) )
    {
        return v;
    }
    if (texture(shadowMap, rayCoordLightSpace.xy).z < rayCoordLightSpace.z - BIAS) {
        v = 0;
    }
    return v;
}

// phase function for anisotropic scattering
float p(vec3 w, vec3 wl) {
    float theta = acos(dot(w,wl));
    float k = 1.55 * g - (0.55 * g * g * g);
    float cosVal = 1 + k * cos(theta);
    float quad = cosVal * cosVal;
    float p = (1 - k) * 0.25 * PI_RCP / quad;
    return p;
}

// calculate the lightintensity of the current sample at position rayPositionLightSpace
float executeRaymarching(vec3 x, float dl, float l, vec3 w, vec3 wl) {
    // fetch whether the current position on the ray is visible form the light's perspective or not
    float v = v(x);
    // get distance of current ray postition to the light source in light view-space
    float d = length(x);
    float dRcp = 1.0/d;

    // calculate anisotropic scattering
    float p = p(w, wl);
    if (!useALS) p = 1;

    // calculate the final light contribution for the sample on the way
    float radiantFluxAttenuation = phi * PI_RCP * 0.25;
    float Li = tau * albedo * radiantFluxAttenuation * v * p;
    float intens = Li * exp(-tau * d)* exp(-tau * l) * dl;
    
    return intens;
}

void main() {

    int blockSize = PIXEL_SIZE * PIXEL_SIZE;

    // get index for the fragment within the pixelblock
    int index = int(texture(noiseMap, passUV).r * 255.0);

    // calculate number of samples
    float totalSampleNum = SAMPLES;
    float sampleNum = SAMPLES / blockSize;

    // calculate complete raymarching distance
    vec4 startPosCameraSpace = texture(worldPosMap, passUV);
    startPosCameraSpace = vec4(min(length(startPosCameraSpace.xyz), 20) * normalize(startPosCameraSpace.xyz), 1.0);

    vec4 startPosLightSpace = viewToLight * startPosCameraSpace;
    vec3 invViewDir = cameraPosLightSpace.xyz - startPosLightSpace.xyz;
    float s = length(invViewDir);

    float dl = s / totalSampleNum;
    float dlb = blockSize * dl;

    // calculate the stepsize of the ray
    vec3 w = normalize(invViewDir);
    vec3 viewDir = w * dlb;

    // offset starting position
    vec3 offset = w * index * dl;
    float o = length(offset);
    vec3 x = startPosLightSpace.xyz + offset;

    // total light contribution accumulated along the ray
    float vli = 0.0f;
    for (float l = s - o - dl; l >= 0;  l -= dlb) {
        x +=  viewDir;
        vec3 wl = -normalize(x);
        vli += executeRaymarching(x, dl, l, w, wl);
    }
    vli /= sampleNum;
    vli = clamp(vli, 0.0f, clampMax);

    gl_FragColor =  vec4(lightColor * vli, 1.0);
}
