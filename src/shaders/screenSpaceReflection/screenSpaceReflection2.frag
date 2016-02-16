#version 430

in vec2 vert_UV;

out vec4 FragColor;

uniform float screenWidth; 
uniform float screenHeight;

uniform float camNearPlane; 
uniform float camFarPlane; 

uniform int user_pixelStepSize; 

uniform bool fadeToEdges;

uniform mat4 projection; 

uniform sampler2D vsPositionTex; 
uniform sampler2D vsNormalTex; 
uniform sampler2D ReflectanceTex; 
uniform sampler2D DepthTex; 
uniform sampler2D DiffuseTex; 

//!< Linearizes a depth value 
//  Source: http://www.geeks3d.com/20091216/geexlab-how-to-visualize-the-depth-buffer-in-glsl/ 
float linearizeDepth(float depth) { 
 float near = camNearPlane; 
 float far = camFarPlane; 
 float linearDepth = (2.0 * near) / (far + near - depth * (far - near)); 
 
 return linearDepth; 
} 

//!< ssr func
vec4 ScreenSpaceReflections(in vec3 vsPosition, in vec3 vsNormal, in vec3 vsReflectionVector){
    
    float factor = dot(vsReflectionVector, vsNormal);

    // Variables
    vec4 reflectedColor = vec4(0.0);
    vec2 pixelsize = 1.0/vec2(screenWidth, screenHeight);

    // Get texture informations
    vec4 csPosition = projection * vec4(vsPosition, 1.0);
    vec3 ndcsPosition = csPosition.xyz / csPosition.w;
    vec3 ssPosition = 0.5 * ndcsPosition + 0.5;

    // Project reflected vector into screen space
    vsReflectionVector += vsPosition;
    vec4 csReflectionVector = projection * vec4(vsReflectionVector, 1.0);
    vec3 ndcsReflectionVector = csReflectionVector.xyz / csReflectionVector.w;
    vec3 ssReflectionVector = 0.5 * ndcsReflectionVector + 0.5;
    ssReflectionVector = normalize(ssReflectionVector - ssPosition);

    vec3 lastSamplePosition;
    vec3 currentSamplePosition;

    float initalStep;
    float pixelStepSize;

    int sampleCount = max(int(screenWidth), int(screenHeight));
    int count = 0;
    
    //optimiert
//        if(optimizedSSR)
//    {
//        // Ray trace
//        initalStep = 1.0/Screen.Height;
//        ssReflectionVector = normalize(ssReflectionVector) * initalStep;
//
//        lastSamplePosition = ssPosition + ssReflectionVector;
//        currentSamplePosition = lastSamplePosition + ssReflectionVector;
//
//        float bigStep = 12 * factor;
//        int sampleCount = max(int(Screen.Height), int(Screen.Width))/int(bigStep);
//        int count = 0;
//
//        while(count < sampleCount)
//        {
//           // Out of screen space --> break
//            if(currentSamplePosition.x < 0.0 || currentSamplePosition.x > 1.0 ||
//               currentSamplePosition.y < 0.0 || currentSamplePosition.y > 1.0 ||
//               currentSamplePosition.z < 0.0 || currentSamplePosition.z > 1.0)
//            {
//                break;
//            }
//            
//            vec2 samplingPosition = currentSamplePosition.xy;
//            float sampledDepth = linearizeDepth( texture(DepthTex, samplingPosition).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[0] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[1] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[2] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[3] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[4] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[5] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[6] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[7] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[8] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[9] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[10] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[11] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[12] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[13] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[14] * 0.001f).z );
                  //sampledDepth += linearizeDepth (texture2D(DepthTex, samplingPosition + sampleOffsets[15] * 0.001f).z );
                  //sampledDepth /= 16;
//
//            float currentDepth = linearizeDepth(currentSamplePosition.z);
//            
//            float delta = abs(currentDepth - sampledDepth);
//
//            // Step ray
//            if(currentDepth < sampledDepth)
//            {
//                lastSamplePosition = currentSamplePosition;
//                currentSamplePosition = lastSamplePosition + ssReflectionVector * bigStep;
//            }
//           else{
//                if(currentDepth > sampledDepth)
//                {
//                   for(float i=0; i < bigStep; i += 1.0 * factor)
//                    {
//                        lastSamplePosition = currentSamplePosition;
//                        currentSamplePosition = lastSamplePosition - ssReflectionVector * 0.05 * i * factor;
//                    }
//
//                }
//                if(delta < 0.015)
//                {
//                    if(toggleGlossy)
//                    {
//                        float diskSize = 0.00001f;
//
//                        reflectedColor = texture2D(DiffuseTex, samplingPosition);
//                       reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 0] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 1] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 2] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 3] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 4] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 5] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 6] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 7] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 8] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[ 9] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[10] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[11] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[12] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[13] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[14] * diskSize);
//                        reflectedColor += texture2D(DiffuseTex, samplingPosition + sampleOffsets[15] * diskSize);
//
//                        reflectedColor /= 17;
//                    }
//                    else
//                    {
//                        reflectedColor = texture2D(DiffuseTex, samplingPosition);
//                    }
//
//                    break;
//                }
//           }           
//            
//            count++;
//        }
//    }
    
    //nichtotimiert
//    else
//    {
    // Ray trace
    initalStep = max(pixelsize.x, pixelsize.y);
    pixelStepSize = user_pixelStepSize;
    ssReflectionVector *= initalStep;

    lastSamplePosition = ssPosition + ssReflectionVector;
    currentSamplePosition = lastSamplePosition + ssReflectionVector;

    while(count < sampleCount){
    	// Out of screen space --> break
        if(currentSamplePosition.x < 0.0 || currentSamplePosition.x > 1.0 ||
           currentSamplePosition.y < 0.0 || currentSamplePosition.y > 1.0 ||
           currentSamplePosition.z < 0.0 || currentSamplePosition.z > 1.0){
        	break;
        }           
        vec2 samplingPosition = currentSamplePosition.xy;
        float sampledDepth = linearizeDepth( texture(DepthTex, samplingPosition).z );
        float currentDepth = linearizeDepth(currentSamplePosition.z);

        if(currentDepth > sampledDepth){   
            float delta = abs(currentDepth - sampledDepth);
            if(delta <= 0.001f){
                float f = currentDepth;
                float blurSize = 30 * f; 
                reflectedColor = texture2D(DiffuseTex, vec2(samplingPosition.x, samplingPosition.y));

                for(float i= - blurSize/2.0; i < blurSize/2.0; i+= 1.0){
                    reflectedColor += texture2D(DiffuseTex, vec2(samplingPosition.x, samplingPosition.y + i * pixelsize.y));
                }
                reflectedColor /= blurSize;
                break;  
            }
        }
        else{
            // Step ray
            lastSamplePosition = currentSamplePosition;
            currentSamplePosition = lastSamplePosition + ssReflectionVector * pixelStepSize;    
        }
        count++;
    }
//    }

    // Fading to screen edges
    vec2 fadeToScreenEdge = vec2(1.0);
    if(fadeToEdges){
        fadeToScreenEdge.x = distance(lastSamplePosition.x , 1.0);
        fadeToScreenEdge.x *= distance(lastSamplePosition.x, 0.0) * 4.0;
        fadeToScreenEdge.y = distance(lastSamplePosition.y, 1.0);
        fadeToScreenEdge.y *= distance(lastSamplePosition.y, 0.0) * 4.0;
    }
    return (reflectedColor * fadeToScreenEdge.x * fadeToScreenEdge.y);
}

//!< main
void main(void){
 //texture information from G-Buffer
 float reflectance = texture(ReflectanceTex, vert_UV).a; 
 vec3 vsPosition = texture(vsPositionTex, vert_UV).xyz; 
 vec3 vsNormal = texture(vsNormalTex, vert_UV).xyz; 
 
 //reflection vector calculation
 //view space calculations 
 vec3 vsEyeVector        = normalize(vsPosition); 
 vec3 vsReflectionVector = normalize(reflect(vsEyeVector, vsNormal));             
 
 bool toggleSSR = true;
 //screen space reflections
 if(toggleSSR){ 
  vec4 color = ScreenSpaceReflections(vsPosition, vsNormal, vsReflectionVector); 
  //color = mix(color, texture(ReflectanceTex, vert_UV), 0.5); 
  FragColor = reflectance * color; 
 } 
}