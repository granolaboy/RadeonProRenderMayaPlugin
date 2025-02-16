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
#include "FireRenderMeshMASH.h"

FireRenderMeshMASH::FireRenderMeshMASH(const FireRenderMesh& rhs, const std::string& uuid, const MObject instancer)
	: FireRenderMesh(rhs, uuid),
	m_Instancer(instancer),
	m_originalFRMesh(rhs)
{ 
	m_SelfTransform.setToIdentity();
}

void FireRenderMeshMASH::SetSelfTransform(const MMatrix& matrix)
{
	m_SelfTransform = matrix;
}

bool FireRenderMeshMASH::IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const
{
	(void)meshPath;
	(void)context;
	MDagPath instancerPath;
	MDagPath::getAPathTo(m_Instancer, instancerPath);
	return instancerPath.isVisible();
}

MMatrix FireRenderMeshMASH::GetSelfTransform()
{
	return m_SelfTransform;
}

bool FireRenderMeshMASH::PreProcessMesh(unsigned int sampleIdx /*= 0*/)
{
	return (FireRenderMesh::PreProcessMesh(sampleIdx));
}

void FireRenderMeshMASH::Rebuild(void)
{
	FireRenderMesh::Rebuild();
}

