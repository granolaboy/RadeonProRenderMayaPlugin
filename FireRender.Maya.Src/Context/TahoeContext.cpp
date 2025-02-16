/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#include "TahoeContext.h"
#include "maya/MColorManagementUtilities.h"
#include "maya/MFileObject.h"

TahoeContext::LoadedPluginMap TahoeContext::m_gLoadedPluginsIDsMap;

TahoeContext::TahoeContext() :
	m_PluginVersion(TahoePluginVersion::RPR1),
	m_PreviewMode(true)
{

}

void TahoeContext::SetPluginEngine(TahoePluginVersion version)
{
	m_PluginVersion = version;
}

rpr_int TahoeContext::GetPluginID(TahoePluginVersion version)
{
	rpr_int pluginId = INCORRECT_PLUGIN_ID;
	LoadedPluginMap::const_iterator it = m_gLoadedPluginsIDsMap.find(version);
	if (it == m_gLoadedPluginsIDsMap.end())
	{
        std::string libName;
        
        if (version == TahoePluginVersion::RPR1)
        {
            libName = "Tahoe64";
        }
        else if (version == TahoePluginVersion::RPR2)
        {
            libName = "Northstar64";
        }
#ifdef OSMac_
        libName = "lib" + libName + ".dylib";
        std::string pathToTahoeDll = "/Users/Shared/RadeonProRender/Maya/lib/" + libName;
        if (0 != access(pathToTahoeDll.c_str(), F_OK))
        {
             pathToTahoeDll = "/Users/Shared/RadeonProRender/lib/" + libName;
        }
		pluginId = rprRegisterPlugin(pathToTahoeDll.c_str());
#elif __linux__
		pluginId = rprRegisterPlugin("libTahoe64.so");
#else
		libName += ".dll";
		pluginId = rprRegisterPlugin(libName.c_str());
#endif

		m_gLoadedPluginsIDsMap[version] = pluginId;
	}
	else
	{
		pluginId = it->second;
	}

	return pluginId;
}

rpr_int TahoeContext::CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext)
{
	rpr_int pluginID = GetPluginID(m_PluginVersion);

	if (pluginID == INCORRECT_PLUGIN_ID)
	{
		MGlobal::displayError("Unable to register Radeon ProRender plug-in.");
		return RPR_ERROR_INVALID_PARAMETER;
	}

	rpr_int plugins[] = { pluginID };
	size_t pluginCount = 1;

	auto cachePath = getShaderCachePath();

	// setup CPU thread count
	std::vector<rpr_context_properties> ctxProperties;
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE);
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE_CMJ);

	int threadCountToOverride = getThreadCountToOverride();
	if ((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) && threadCountToOverride > 0)
	{
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CPU_THREAD_LIMIT);
		ctxProperties.push_back((void*)(size_t)threadCountToOverride);
	}

	ctxProperties.push_back((rpr_context_properties)0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	int res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#else
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#endif

	return res;
}

void TahoeContext::setupContextContourMode(const FireRenderGlobalsData& fireRenderGlobalsData, int createFlags, bool disableWhiteBalance /*= false*/)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	if (m_PluginVersion == TahoePluginVersion::RPR2)
	{
		// contour must be set before scene creation
		bool isContourModeOn = fireRenderGlobalsData.contourIsEnabled && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU);

		if (isContourModeOn)
		{
			frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_GPUINTEGRATOR, "gpucontour");
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_OBJECTID, fireRenderGlobalsData.contourUseObjectID);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_MATERIALID, fireRenderGlobalsData.contourUseMaterialID);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_NORMAL, fireRenderGlobalsData.contourUseShadingNormal);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_OBJECTID, fireRenderGlobalsData.contourLineWidthObjectID);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_MATERIALID, fireRenderGlobalsData.contourLineWidthMaterialID);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_NORMAL, fireRenderGlobalsData.contourLineWidthShadingNormal);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_NORMAL_THRESHOLD, fireRenderGlobalsData.contourNormalThreshold);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_ANTIALIASING, fireRenderGlobalsData.contourAntialiasing);
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_DEBUG_ENABLED, fireRenderGlobalsData.contourIsDebugEnabled);
			checkStatus(frstatus);
		}
	}
}

void TahoeContext::setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_PDF_THRESHOLD, 0.0000f);
	checkStatus(frstatus);

	if (GetRenderType() == RenderType::Thumbnail)
	{
		updateTonemapping(fireRenderGlobalsData, false);
		return;
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_RADIANCE_CLAMP, fireRenderGlobalsData.giClampIrradiance ? fireRenderGlobalsData.giClampIrradianceValue : FLT_MAX);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_TEXTURE_COMPRESSION, fireRenderGlobalsData.textureCompression);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_TILE_SIZE, fireRenderGlobalsData.adaptiveTileSize);
	checkStatus(frstatus);

	bool velocityAOVMotionBlur = !fireRenderGlobalsData.velocityAOVMotionBlur;
	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_BEAUTY_MOTION_BLUR, velocityAOVMotionBlur);

	if (GetRenderType() == RenderType::ProductionRender) // production (final) rendering
	{
		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_THRESHOLD, fireRenderGlobalsData.adaptiveThreshold);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RENDER_MODE, fireRenderGlobalsData.renderMode);
		checkStatus(frstatus);

		if (fireRenderGlobalsData.contourIsEnabled)
		{
			setSamplesPerUpdate(1);
		}
		else
		{
			setSamplesPerUpdate(fireRenderGlobalsData.samplesPerUpdate);
		}

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, fireRenderGlobalsData.maxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, fireRenderGlobalsData.maxRayDepthDiffuse);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY, fireRenderGlobalsData.maxRayDepthGlossy);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_REFRACTION, fireRenderGlobalsData.maxRayDepthRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, fireRenderGlobalsData.maxRayDepthGlossyRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_SHADOW, fireRenderGlobalsData.maxRayDepthShadow);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_MIN_SPP, fireRenderGlobalsData.completionCriteriaFinalRender.completionCriteriaMinIterations);
		checkStatus(frstatus);

		// if Deep Exr enabled
		if (fireRenderGlobalsData.aovs.IsAOVActive(RPR_AOV_DEEP_COLOR))
		{
			MDistance distance(fireRenderGlobalsData.deepEXRMergeZThreshold, MDistance::uiUnit());

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DEEP_SUBPIXEL_MERGE_Z_THRESHOLD, distance.asMeters());
			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DEEP_GPU_ALLOCATION_LEVEL, 4);
			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DEEP_COLOR_ENABLED, 1);
		}
	}
	else if (isInteractive())
	{
		setSamplesPerUpdate(1);
        
        if (m_PluginVersion == TahoePluginVersion::RPR2)
        {
            SetIterationsPowerOf2Mode(true);
        }

		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_THRESHOLD, fireRenderGlobalsData.adaptiveThresholdViewport);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RENDER_MODE, fireRenderGlobalsData.viewportRenderMode);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, fireRenderGlobalsData.viewportMaxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_REFRACTION, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_SHADOW, fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_MIN_SPP, fireRenderGlobalsData.completionCriteriaViewport.completionCriteriaMinIterations);
		checkStatus(frstatus);
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_RAY_CAST_EPISLON, fireRenderGlobalsData.raycastEpsilon);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_IMAGE_FILTER_TYPE, fireRenderGlobalsData.filterType);
	checkStatus(frstatus);

	rpr_material_node_input filterAttrName = RPR_CONTEXT_IMAGE_FILTER_BOX_RADIUS;
	switch (fireRenderGlobalsData.filterType)
	{
	case 2:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_TRIANGLE_RADIUS;
		break;
	}
	case 3:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_GAUSSIAN_RADIUS;
		break;
	}
	case 4:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_MITCHELL_RADIUS;
		break;
	}
	case 5:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_LANCZOS_RADIUS;
		break;
	}
	case 6:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS;
		break;
	}
	default:
		break;
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, filterAttrName, fireRenderGlobalsData.filterSize);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_METAL_PERFORMANCE_SHADER, fireRenderGlobalsData.useMPS ? 1 : 0);
	checkStatus(frstatus);

	updateTonemapping(fireRenderGlobalsData, disableWhiteBalance);

	if (m_PluginVersion == TahoePluginVersion::RPR2)
	{
		frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_TEXTURE_CACHE_PATH, fireRenderGlobalsData.textureCachePath.asChar());
		checkStatus(frstatus);

		// OCIO
		const std::map<std::string, std::string>& eVars = EnvironmentVarsWrapper<char>::GetEnvVarsTable();
		auto envOCIOPath = eVars.find("OCIO");
		if (envOCIOPath != eVars.end())
		{
			MFileObject path;
			path.setRawFullName(envOCIOPath->second.c_str());
			MString setupCommand = MString("colorManagementPrefs -e -configFilePath \"") + path.resolvedFullName() + MString("\";");
			MGlobal::executeCommand(setupCommand);
			MGlobal::executeCommand(MString("colorManagementPrefs -e -cmEnabled 1;"));
			MGlobal::executeCommand(MString("colorManagementPrefs -e -cmConfigFileEnabled 1;"));
		}

		MStatus colorManagementStatus;
		int isColorManagementOn = 0;
		colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -cmEnabled;"), isColorManagementOn);

		int isConfigFileEnable = 0;
		colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -cmConfigFileEnabled;"), isConfigFileEnable);

		if ((isColorManagementOn > 0) && (isConfigFileEnable > 0))
		{
			MString configFilePath;
			colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -cfp;"), configFilePath);

			MString renderingSpaceName;
			colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -rsn;"), renderingSpaceName);

			frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_CONFIG_PATH, configFilePath.asChar());
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_RENDERING_COLOR_SPACE, renderingSpaceName.asChar());
			checkStatus(frstatus);
		}
		else
		{
			frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_CONFIG_PATH, "");
			checkStatus(frstatus);

			frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_RENDERING_COLOR_SPACE, "");
			checkStatus(frstatus);
		}
	}
}

void TahoeContext::updateTonemapping(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_TEXTURE_GAMMA, fireRenderGlobalsData.textureGamma);
	checkStatus(frstatus);

	// Release existing effects
	if (white_balance)
	{
		context.Detach(white_balance);
		white_balance.Reset();
	}

	if (simple_tonemap)
	{
		context.Detach(simple_tonemap);
		simple_tonemap.Reset();
	}

	if (tonemap)
	{
		context.Detach(tonemap);
		tonemap.Reset();
	}

	if (normalization)
	{
		context.Detach(normalization);
		normalization.Reset();
	}

	if (gamma_correction)
	{
		context.Detach(gamma_correction);
		gamma_correction.Reset();
	}

	// At least one post effect is required for frame buffer resolve to
	// work, which is required for OpenGL interop. Frame buffer normalization
	// should be applied before other post effects.
	// Also gamma effect requires normalization when tonemapping is not used.
	normalization = frw::PostEffect(context, frw::PostEffectTypeNormalization);
	context.Attach(normalization);

	// Create new effects
	switch (fireRenderGlobalsData.toneMappingType)
	{
	case 0:
		break;

	case 1:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_LINEAR);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE, fireRenderGlobalsData.toneMappingLinearScale);
		break;

	case 2:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_PHOTOLINEAR);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY, fireRenderGlobalsData.toneMappingPhotolinearSensitivity);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP, fireRenderGlobalsData.toneMappingPhotolinearFstop);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE, fireRenderGlobalsData.toneMappingPhotolinearExposure);
		break;

	case 3:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_AUTOLINEAR);
		break;

	case 4:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_MAXWHITE);
		break;

	case 5:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_REINHARD02);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE, fireRenderGlobalsData.toneMappingReinhard02Prescale);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE, fireRenderGlobalsData.toneMappingReinhard02Postscale);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN, fireRenderGlobalsData.toneMappingReinhard02Burn);
		break;

	case 6:
		if (!simple_tonemap)
		{
			simple_tonemap = frw::PostEffect(context, frw::PostEffectTypeSimpleTonemap);
			simple_tonemap.SetParameter("tonemap", fireRenderGlobalsData.toneMappingSimpleTonemap);
			simple_tonemap.SetParameter("exposure", fireRenderGlobalsData.toneMappingSimpleExposure);
			simple_tonemap.SetParameter("contrast", fireRenderGlobalsData.toneMappingSimpleContrast);
			context.Attach(simple_tonemap);
		}
		break;

	default:
		break;
	}

	// This block is required for gamma functionality
	if (fireRenderGlobalsData.applyGammaToMayaViews)
	{
		gamma_correction = frw::PostEffect(context, frw::PostEffectTypeGammaCorrection);
		context.Attach(gamma_correction);
	}

	bool wbApplied = fireRenderGlobalsData.toneMappingWhiteBalanceEnabled && !disableWhiteBalance;
	if (wbApplied)
	{
		if (!white_balance)
		{
			white_balance = frw::PostEffect(context, frw::PostEffectTypeWhiteBalance);
			context.Attach(white_balance);
		}

		float temperature = fireRenderGlobalsData.toneMappingWhiteBalanceValue;
		white_balance.SetParameter("colorspace", RPR_COLOR_SPACE_SRGB); // check: Max uses Adobe SRGB here
		white_balance.SetParameter("colortemp", temperature);
	}

	bool setDisplayGamma = fireRenderGlobalsData.applyGammaToMayaViews || simple_tonemap || tonemap || wbApplied;
	float displayGammaValue = setDisplayGamma ? fireRenderGlobalsData.displayGamma : 1.0f;
	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DISPLAY_GAMMA, displayGammaValue);
	checkStatus(frstatus);
}

bool TahoeContext::needResolve() const
{
	return bool(white_balance) || bool(simple_tonemap) || bool(tonemap) || bool(normalization) || bool(gamma_correction);
}

bool TahoeContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return quality == RenderQuality::RenderQualityFull;
}

bool TahoeContext::IsDenoiserSupported() const
{
	return true;
}

bool TahoeContext::ShouldForceRAMDenoiser() const
{
	return m_PluginVersion == TahoePluginVersion::RPR2;
}

bool TahoeContext::IsDisplacementSupported() const
{
	return true;
}

bool TahoeContext::IsHairSupported() const
{
	return true;
}

bool TahoeContext::IsVolumeSupported() const
{
	return true;
}

bool TahoeContext::IsNorthstarVolumeSupported() const
{
	return m_PluginVersion == TahoePluginVersion::RPR2;
}

bool TahoeContext::IsAOVSupported(int aov) const 
{
	if (aov >= RPR_AOV_MAX)
	{
		return false;
	}

	return (aov != RPR_AOV_VIEW_SHADING_NORMAL) && (aov != RPR_AOV_COLOR_RIGHT);
}

bool TahoeContext::IsPhysicalLightTypeSupported(PLType lightType) const
{
	if (lightType == PLTDisk || lightType == PLTSphere)
	{
		return m_PluginVersion == TahoePluginVersion::RPR2;
	}

	return true;
}

bool TahoeContext::IsGLInteropEnabled() const
{
	return m_PluginVersion == TahoePluginVersion::RPR1;
}

bool TahoeContext::MetalContextAvailable() const
{
    return true;
}

bool TahoeContext::IsDeformationMotionBlurEnabled() const
{
	return TahoeContext::IsGivenContextRPR2(this);
}

void TahoeContext::SetRenderUpdateCallback(RenderUpdateCallback callback, void* data)
{
	if (m_PluginVersion == TahoePluginVersion::RPR2)
	{
		GetScope().Context().SetUpdateCallback((void*)callback, data);
	}
}

bool TahoeContext::IsGivenContextRPR2(const FireRenderContext* pContext)
{
	const TahoeContext* pTahoeContext = dynamic_cast<const TahoeContext*> (pContext);

	if (pTahoeContext == nullptr)
	{
		return false;
	}

	return pTahoeContext->m_PluginVersion == TahoePluginVersion::RPR2;
}

void TahoeContext::AbortRender()
{
	if (m_PluginVersion == TahoePluginVersion::RPR2)
	{
		GetScope().Context().AbortRender();
	}
}

void TahoeContext::SetupPreviewMode()
{
	if (m_PluginVersion == TahoePluginVersion::RPR1)
	{
		RenderType renderType = GetRenderType();
		int preview = 0;

		if ((renderType == RenderType::ViewportRender) ||
			(renderType == RenderType::IPR) ||
			(renderType == RenderType::Thumbnail))
		{
			preview = 1;
		}

		SetPreviewMode(preview);
	}
}

void TahoeContext::OnPreRender()
{
	RenderType renderType = GetRenderType();

	if ( (m_PluginVersion == TahoePluginVersion::RPR1) || 
		((renderType != RenderType::ViewportRender) &&
			(renderType != RenderType::IPR)))
	{
		return;
	}

	const int previewModeLevel = 2;
	if (m_restartRender)
	{
		m_PreviewMode = true;
		SetPreviewMode(previewModeLevel);
	}

	if (m_currentFrame == 2 && m_PreviewMode)
	{
		SetPreviewMode(0);
		m_PreviewMode = false;
		m_restartRender = true;
	}
	else if (m_currentFrame < 2 && m_PreviewMode)
	{
		SetPreviewMode(previewModeLevel);
	}
}

int TahoeContext::GetAOVMaxValue()
{
	bool isRPR20 = TahoeContext::IsGivenContextRPR2(this);

	if (isRPR20)
		return RPR_AOV_MAX;

	return 0x20;
}

