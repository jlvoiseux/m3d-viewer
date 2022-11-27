#include "pch.h"
#include "ModelExtended.h"

#include "Effects.h"
#include "VertexTypes.h"

#include "DirectXHelpers.h"
#include "DescriptorHeap.h"
#include "CommonStates.h"

#define M3D_CPPWRAPPER
#define M3D_IMPLEMENTATION
#include "M3d.h"
#include "PlatformHelpers.h"
#include "BinaryReader.h"

#define MAX_MESH_NAME 100

using namespace DirectX;
using Microsoft::WRL::ComPtr;

template<size_t sizeOfBuffer>
inline void ASCIIToWChar(wchar_t(&buffer)[sizeOfBuffer], const char* ascii)
{
    MultiByteToWideChar(CP_UTF8, 0, ascii, -1, buffer, sizeOfBuffer);
}

_Use_decl_annotations_
std::unique_ptr<Model> __cdecl ModelExtended::CreateFromM3D(
	ID3D12Device* device, 
	const uint8_t* meshData,
	size_t idataSize,
	ModelLoaderFlags flags)
{
	if (!meshData)
		throw std::invalid_argument("meshData cannot be null");

	std::vector<unsigned char> buffer(meshData, meshData + idataSize);
	M3D::Model* m3dModel = new M3D::Model(buffer, NULL, NULL);

    auto model = std::make_unique<Model>();
    model->meshes.reserve(1);
    auto mesh = std::make_shared<ModelMesh>();
    wchar_t meshName[MAX_MESH_NAME] = {};
    ASCIIToWChar(meshName, m3dModel->getName().c_str());
    mesh->name = meshName;
    auto part = new ModelMeshPart(0);

    std::vector<m3dm_t> m3dMaterials = m3dModel->getMaterials();
    std::vector<ModelMaterialInfo> materials;
    materials.resize(m3dMaterials.size());
    for (auto it = begin(m3dMaterials); it != end(m3dMaterials); ++it) {
        auto& mat = materials[0];
        mat.name = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(it->name);
        mat.ambientColor = XMFLOAT3(1.f, 1.f, 1.f);
        mat.diffuseColor = XMFLOAT3(1.f, 1.f, 1.f);
        mat.specularColor = XMFLOAT3(0.3f, 0.3f, 0.3f);
        mat.specularPower = 360;
        mat.alphaValue = 1.f;
        mat.diffuseTextureIndex = 0; // HARDCODED
        mat.samplerIndex = 4;
    }
    // Copy the materials and texture names into contiguous arrays
    model->materials = std::move(materials);

    std::vector<m3dtx_t> m3dTextures = m3dModel->getTextures();
    std::map<std::wstring, int> textureDictionary;
    const std::string fileFormat = ".png";
    int texCount = 0;
    for (auto it = begin(m3dTextures); it != end(m3dTextures); ++it) {
        std::string texName = it->name + fileFormat;
        std::wstring wTexName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(texName);
        textureDictionary[wTexName] = texCount;
        texCount++;
    }
    model->textureNames.resize(textureDictionary.size());
    for (auto texture = std::cbegin(textureDictionary); texture != std::cend(textureDictionary); ++texture)
    {
        model->textureNames[static_cast<size_t>(texture->second)] = texture->first;
    }
    
    
    std::vector<m3dv_t> m3dVerts = m3dModel->getVertices();
    std::vector<m3df_t> m3dTris = m3dModel->getFace();
    std::vector<m3dti_t> m3dTex = m3dModel->getTextureMap();
    const size_t vNum = m3dVerts.size();
    const size_t stride = sizeof(VertexPositionNormalTexture);
    const size_t tNum = m3dTris.size();

    part->materialIndex = 0;
    part->indexCount = tNum * 3; // Each triangle has 3 vertices
    part->startIndex = 0;
    part->vertexOffset = 0;
    part->vertexStride = static_cast<UINT>(stride);
    part->vertexCount = vNum;
    part->indexFormat = DXGI_FORMAT_R16_UINT;

    // Vertex buffer
    std::vector<VertexPositionNormalTexture> vertices;
    std::map<uint16_t, uint16_t> m3dVertIndexMap;

    // Index Buffer
    std::vector<uint16_t> indices;
    for (auto it = begin(m3dTris); it != end(m3dTris); ++it) {
        for (int i : {0, 1, 2}) {
            uint16_t currVert = it->vertex[i];
            uint16_t currNorm = it->normal[i];
            uint16_t currTexcoord = it->texcoord[i];
            std::map<uint16_t, uint16_t>::iterator mapIt = m3dVertIndexMap.find(currVert);
            if (mapIt == m3dVertIndexMap.end()) {
                uint16_t newIndex = m3dVertIndexMap.size();
                indices.push_back(newIndex);
                vertices.push_back(VertexPositionNormalTexture(
                    XMFLOAT3(m3dVerts[currVert].x, m3dVerts[currVert].y, m3dVerts[currVert].z),
                    XMFLOAT3(m3dVerts[currNorm].x, m3dVerts[currNorm].y, m3dVerts[currNorm].z),
                    XMFLOAT2(m3dTex[currTexcoord].u, m3dTex[currTexcoord].v)
                ));
                m3dVertIndexMap[currVert] = newIndex;
            }
            else {
                indices.push_back(mapIt->second);
            }
        }
    }
    part->indexBufferSize = tNum * 6; // Each triangle has 3 vertices stored in 2 bytes
    part->vertexBufferSize = stride * vertices.size();
    part->indexBuffer = GraphicsMemory::Get(device).Allocate(part->indexBufferSize);
    memcpy(part->indexBuffer.Memory(), indices.data(), part->indexBufferSize);
    part->vertexBuffer = GraphicsMemory::Get(device).Allocate(part->vertexBufferSize);
    memcpy(part->vertexBuffer.Memory(), vertices.data(), part->vertexBufferSize);

    
    part->vbDecl = std::make_shared<ModelMeshPart::InputLayoutCollection>(VertexPositionNormalTexture::InputLayout.pInputElementDescs,
        VertexPositionNormalTexture::InputLayout.pInputElementDescs + VertexPositionNormalTexture::InputLayout.NumElements);

    mesh->opaqueMeshParts.emplace_back(part);
    model->meshes.emplace_back(mesh);

    return model;
}

_Use_decl_annotations_
std::unique_ptr<Model> ModelExtended::CreateFromM3D(
    ID3D12Device* device,
    const wchar_t* szFileName,
    ModelLoaderFlags flags)
{
    size_t dataSize = 0;
    std::unique_ptr<uint8_t[]> data;
    HRESULT hr = BinaryReader::ReadEntireFile(szFileName, data, &dataSize);
    if (FAILED(hr))
    {
        DebugTrace("ERROR: CreateFromSDKMESH failed (%08X) loading '%ls'\n",
            static_cast<unsigned int>(hr), szFileName);
        throw std::runtime_error("CreateFromSDKMESH");
    }

    auto model = CreateFromM3D(device, data.get(), dataSize, flags);

    model->name = szFileName;

    return model;
}