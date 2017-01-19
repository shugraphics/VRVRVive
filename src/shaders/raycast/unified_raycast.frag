#version 430
// One Raycasting Shader to Rule them all!

////////////////////////////////     DEFINES      ////////////////////////////////
/*********** LIST OF POSSIBLE DEFINES ***********
	ALPHA_SCALE <float>
	ERT_THRESHOLD <float>
	EMISSION_ABSORPTION_RAW
		EMISSION_SCALE <float>
		ABSORPTION_SCALE <float>
	FIRST_HIT
	LEVEL_OF_DETAIL
	OCCLUSION_MAP
	RANDOM_OFFSET
	SCENE_DEPTH
***********/

#ifndef ALPHA_SCALE
#define ALPHA_SCALE 20.0
#endif

#ifdef RANDOM_OFFSET 
float rand(vec2 co) { //!< http://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
#endif

#ifndef ERT_THRESHOLD
#define ERT_THRESHOLD 0.99
#endif

#ifdef EMISSION_ABSORPTION_RAW
	#ifndef EMISSION_SCALE
	#define EMISSION_SCALE 1.0
	#endif

	#ifndef ABSORPTION_SCALE
	#define ABSORPTION_SCALE 10.0
	#endif
#endif
///////////////////////////////////////////////////////////////////////////////////

// in-variables
in vec2 passUV;

// textures
uniform sampler1D transferFunctionTex;
uniform sampler2D back_uvw_map;   // uvw coordinates map of back  faces
uniform sampler2D front_uvw_map;   // uvw coordinates map of front faces
uniform sampler3D volume_texture; // volume 3D integer texture sampler

#ifdef OCCLUSION_MAP
	uniform sampler2D occlusion_map;   // uvw coordinates map of occlusion map
	uniform sampler2D occlusion_clip_frustum_back;   // uvw coordinates map of occlusion map
	uniform sampler2D occlusion_clip_frustum_front;   // uvw coordinates map of occlusion map
	
	//uniform mat4 uScreenToOcclusionView; // view that produced the first hit map on which the occlusion_map is based
	//uniform mat4 uOcclusionViewToTexture; // from occlusion view to texture
#endif

#ifdef SCENE_DEPTH
	uniform sampler2D scene_depth_map;   // depth map of scene
#endif

////////////////////////////////     UNIFORMS      ////////////////////////////////
// color mapping related uniforms 
uniform float uWindowingRange;  // windowing value range
uniform float uWindowingMinVal; // windowing lower bound

// ray traversal related uniforms
uniform float uStepSize;	// ray sampling step size

#ifdef LEVEL_OF_DETAIL
	uniform float uLodMaxLevel; // factor with wich depth influences sampled LoD and step size 
	uniform float uLodBegin;    // depth at which lod begins to degrade
	uniform float uLodRange;    // depth at which lod begins to degrade
#endif

// auxiliary matrices
uniform mat4 uViewToTexture;
uniform mat4 uScreenToTexture;
uniform mat4 uProjection;

///////////////////////////////////////////////////////////////////////////////////

// out-variables
layout(location = 0) out vec4 fragColor;

#ifdef FIRST_HIT
	layout(location = 1) out vec4 fragFirstHit;
#endif
/**
 * @brief Struct of a volume sample point
 */
struct VolumeSample
{
	float value; // scalar intensity
	vec3 uvw;  // uvw coordinates
};

/**
 * @brief Struct of a volume sample point
 */
struct RaycastResult
{
	vec4 color;  // end color
	vec4 firstHit; // position of first non-zero alpha hit
	vec4 lastHit; // position of last non-zero alpha hit
};

#ifdef EMISSION_ABSORPTION_RAW
	/**
	* @brief 'transfer-function' applied to value, no premultiplied alpha
	*/
	vec4 transferFunctionRaw(float value)
	{
		// linear mapping to grayscale color [0,1]
		float rel = (value - uWindowingMinVal) / uWindowingRange;
		float clamped =	max(0.0, min(1.0, rel));
		vec4 color = texture(transferFunctionTex, clamped);
		return color;
	}
#endif

/**
* @brief 'transfer-function' applied to value.
* @param value to be mapped to a color
* @param stepSize used for alpha scaling
*/
vec4 transferFunction(float value, float stepSize)
{
	// linear mapping to grayscale color [0,1]
	float rel = (value - uWindowingMinVal) / uWindowingRange;
	float clamped =	max(0.0, min(1.0, rel));
	
	vec4 color = texture(transferFunctionTex, clamped);
	color.a *= ALPHA_SCALE * stepSize;
	color.rgb *= (color.a);

	return color;
}

/**
 * @brief retrieve color for a front-to-back raycast between two points in the volume
 * 
 * @param startUVW start uvw coordinates
 * @param endUVW end uvw coordinates
 * @param stepSize of ray traversal
 * @param startDepth of ray (linearized or not)
 * @param endDepth of ray (linearized or not)
 * 
 * @return RaycastResult consisiting of accumulated color, first hit and last hit
 */
RaycastResult raycast(vec3 startUVW, vec3 endUVW, float stepSize, float startDepth, float endDepth)
{
	float parameterStepSize = stepSize / length(endUVW - startUVW); // parametric step size (scaled to 0..1)
	
	RaycastResult result;
	result.color = vec4(0);
	result.firstHit = vec4(0.0);
	result.lastHit = vec4(endUVW, endDepth);
	vec4 curColor = vec4(0);

	// traverse ray front to back rendering
	float t = 0.0;
	#ifdef RANDOM_OFFSET 
		t = 0.002 * 2.0 * rand(passUV);
	#endif
	while( t < 1.0 + (0.5 * parameterStepSize) )
	{
		vec3 curUVW = mix( startUVW, endUVW, t);
		
		float curDepth = mix( startDepth, endDepth, t);

		VolumeSample curSample;
		curSample.uvw   = curUVW;
		
		#ifdef LEVEL_OF_DETAIL
			float curLod = max(0.0, min(1.0, ((curDepth - uLodBegin) / uLodRange) ) ) * uLodMaxLevel; // bad approximation, but idc for now
			float curStepSize = stepSize * pow(2.0, curLod);
			parameterStepSize = curStepSize / length(endUVW - startUVW); // parametric step size (scaled to 0..1)
			curSample.value = textureLod(volume_texture, curUVW, curLod).r;
		#else
			float curStepSize = stepSize;
			curSample.value = texture(volume_texture, curUVW).r;
		#endif

		// retrieve current sample
		#ifdef EMISSION_ABSORPTION_RAW
			float distanceStepSize = parameterStepSize * (endDepth - startDepth); // distance of a step
			vec4 sampleEmissionAbsorption = transferFunctionRaw( curSample.value );
			sampleEmissionAbsorption.rgb = sampleEmissionAbsorption.rgb * EMISSION_SCALE;
			sampleEmissionAbsorption.a = pow(sampleEmissionAbsorption.a * ABSORPTION_SCALE, 2.0); // to scale a bit within real numbers
			float sampleAlpha = 1.0 - exp( - sampleEmissionAbsorption.a * distanceStepSize );

			vec4 sampleColor = vec4(sampleEmissionAbsorption.rgb * (sampleAlpha), sampleAlpha);
		#else
			vec4 sampleColor = transferFunction(curSample.value, curStepSize );
		#endif

		curColor.rgb = (1.0 - curColor.a) * (sampleColor.rgb) + curColor.rgb;
		curColor.a = (1.0 - curColor.a) * sampleColor.a + curColor.a;

		#ifdef FIRST_HIT// first hit?
			if (result.firstHit.a == 0.0 && sampleColor.a > 0.001)
			{
				result.firstHit.rgb = curUVW;
				result.firstHit.a = curDepth;
			} 
		#endif

		// early ray-termination
		if (curColor.a > ERT_THRESHOLD)
		{
			break;
		}

		t += parameterStepSize;
	}

	// return emission absorbtion result
	result.color = curColor;
	return result;
}

/** 
*   @param screenPos screen space position in [0..1]
**/
vec4 getViewCoord( vec3 screenPos )
{
	vec4 unProject = inverse(uProjection) * vec4( (screenPos * 2.0) - 1.0 , 1.0);
	unProject /= unProject.w;
	
	return unProject;
}

void main()
{
	// define ray start and end points in volume
	vec4 uvwStart = texture( front_uvw_map, passUV );
	vec4 uvwEnd   = texture( back_uvw_map,  passUV );

	if (uvwEnd.a == 0.0) { 
		discard; 
	} //invalid pixel
	
	if( uvwStart.a == 0.0 ) // only back-uvws visible (camera inside volume)
	{
		uvwStart = uScreenToTexture * vec4(passUV, 0, 1); // clamp to near plane
		uvwStart.a = 0.0;
	}

	#ifdef OCCLUSION_MAP
		vec4 clipFrustumFront = texture(occlusion_clip_frustum_front, passUV);
		clipFrustumFront.rgb = (uViewToTexture * getViewCoord( vec3(passUV, clipFrustumFront.a) ) ).xyz;
		float firstHit = 0.0;
		if ( clipFrustumFront.a > uvwStart.a ) // there is unknown volume space between proxy geom and frustum map
		{
			RaycastResult raycastResult = raycast( 
			uvwStart.rgb, 			// ray start
			clipFrustumFront.rgb,   // ray end
			uStepSize    			// sampling step size
			, length( getViewCoord(vec3(passUV, uvwStart.a) ).xyz)
			, length( getViewCoord(vec3(passUV, clipFrustumFront.a) ).xyz)
			);

			fragColor = raycastResult.color;
			firstHit = raycastResult.firstHit.a;
		}

		//vec4 clipFrustumBack = texture(occlusion_clip_frustum_back, passUV);
		// not as important...

		// check uvw coords against occlusion map
		vec4 uvwOcclusion = texture(occlusion_map, passUV);
		if (uvwOcclusion.x < 1.0 && uvwOcclusion.x < uvwEnd.a && uvwOcclusion.x > uvwStart.a) // found starting point in front of back face but in back of front face
		{
			// compute uvw from depth value
			uvwStart.xyz = (uViewToTexture * getViewCoord( vec3(passUV, uvwOcclusion.x) ) ).xyz;
			uvwStart.a = uvwOcclusion.x;
		}
	#endif

	#ifdef SCENE_DEPTH
		// check uvw coords against scene depth map, 
		float scene_depth = texture(scene_depth_map, passUV).x;
		if (scene_depth < uvwStart.a) 
		{
			gl_FragDepth = uvwStart.a;
			return;
		}
		if (scene_depth < uvwEnd.a)
		{
			float endDepth = max(scene_depth, uvwStart.a);
			uvwEnd = uViewToTexture * getViewCoord( vec3(passUV, endDepth ) );
			uvwEnd.a = endDepth;
		}
	#endif

	// linearize depth
	float startDistance = length(getViewCoord(vec3(passUV,uvwStart.a)).xyz);
	float endDistance   = length(getViewCoord(vec3(passUV,uvwEnd.a)).xyz);
	//float startDistance = uvwStart.a;
	//float endDistance   = uvwEnd.a;

	// EA-raycasting
	RaycastResult raycastResult = raycast( 
		uvwStart.rgb, 			// ray start
		uvwEnd.rgb,   			// ray end
		uStepSize    			// sampling step size
		, startDistance
		, endDistance
		);

	// final color (front to back)
	fragColor.rgb = (1.0 - fragColor.a) * (raycastResult.color.rgb) + fragColor.rgb;
	fragColor.a   = (1.0 - fragColor.a) * raycastResult.color.a + fragColor.a;

	#ifdef FIRST_HIT
		#ifdef OCCLUSION_MAP
		if (firstHit != 0.0) // first hit happened even before raycasting within occlusion frustum
		{
			raycastResult.firstHit.a = firstHit;
		}
		#endif
		if (raycastResult.firstHit.a > 0.0)
		{
			fragFirstHit.xyz = raycastResult.firstHit.xyz; // uvw coords
			vec4 firstHitProjected = uProjection * inverse(uViewToTexture) * vec4( raycastResult.firstHit.xyz, 1.0);
			fragFirstHit.a = max( (firstHitProjected.z / firstHitProjected.w) * 0.5 + 0.5, 0.0 ); // ndc to depth
			
			gl_FragDepth = max(fragFirstHit.a, 0.0);
		}
		else
		{
			fragFirstHit = uvwEnd;
			gl_FragDepth = 1.0;
		}
	#endif
}
