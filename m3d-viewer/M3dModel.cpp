#include "pch.h"

#include "M3dModel.h"

#include "Effects.h"
#include "VertexTypes.h"

#include "DirectXHelpers.h"
#include "DescriptorHeap.h"
#include "CommonStates.h"

#define M3D_CPPWRAPPER
#define M3D_IMPLEMENTATION
#include "M3d.h"
#include <iostream>

#define MAX_MESH_NAME 100

using namespace DirectX;

std::wstring StringToWString(std::string input)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input);
}

M3dModel::M3dModel(ID3D12Device* device, const wchar_t* szFileName)
{
    size_t dataSize = 0;
    std::unique_ptr<uint8_t[]> data;
    HRESULT hr = BinaryReader::ReadEntireFile(szFileName, data, &dataSize);
    if (FAILED(hr))
    {
        DebugTrace("ERROR: Parsing M3D failed (%08X) loading '%ls'\n",
            static_cast<unsigned int>(hr), szFileName);
        throw std::runtime_error("M3dModel");
    }
    const uint8_t* meshData = data.get();
    std::vector<unsigned char> buffer(meshData, meshData + dataSize);
    device_ = device;
    m3dModel_ = new M3D::Model(buffer, NULL, NULL);
    name_ = szFileName;
    animTime_ = 0;
}

std::unique_ptr<Model> M3dModel::BuildDXTKModel()
{

    M3D::Model* m3dModel = static_cast<M3D::Model*>(m3dModel_);
    std::vector<m3dm_t> m3dMaterials = m3dModel->getMaterials();
    std::vector<m3dtx_t> m3dTextures = m3dModel->getTextures();
    std::vector<m3dv_t> m3dVerts = m3dModel->getVertices();
    std::vector<m3df_t> m3dTris = m3dModel->getFace();
    std::vector<m3dti_t> m3dTex = m3dModel->getTextureMap();

    auto dxtkModel = std::make_unique<Model>();
    dxtkModel->meshes.reserve(1);
    auto mesh = std::make_shared<ModelMesh>();
    wchar_t meshName[MAX_MESH_NAME] = {};
    mesh->name = name_;
    auto part = new ModelMeshPart(0);

    std::vector<Model::ModelMaterialInfo> materials;
    materials.resize(m3dMaterials.size());
    for (auto it = begin(m3dMaterials); it != end(m3dMaterials); ++it) {
        auto& mat = materials[0];
        mat.name = StringToWString(it->name);
        mat.ambientColor = XMFLOAT3(1.f, 1.f, 1.f);
        mat.diffuseColor = XMFLOAT3(1.f, 1.f, 1.f);
        mat.specularColor = XMFLOAT3(0.3f, 0.3f, 0.3f);
        mat.specularPower = 360;
        mat.alphaValue = 1.f;
        mat.diffuseTextureIndex = 0; // HARDCODED
        mat.samplerIndex = 4;
    }
    // Copy the materials and texture names into contiguous arrays
    dxtkModel->materials = std::move(materials);

    std::map<std::wstring, int> textureDictionary;
    const std::string fileFormat = ".png";
    int texCount = 0;
    for (auto it = begin(m3dTextures); it != end(m3dTextures); ++it) {
        std::string texName = it->name + fileFormat;
        std::wstring wTexName = StringToWString(texName);
        textureDictionary[wTexName] = texCount;
        texCount++;
    }
    dxtkModel->textureNames.resize(textureDictionary.size());
    for (auto texture = std::cbegin(textureDictionary); texture != std::cend(textureDictionary); ++texture)
    {
        dxtkModel->textureNames[static_cast<size_t>(texture->second)] = texture->first;
    }

    const size_t stride = sizeof(VertexPositionNormalTexture);

    part->materialIndex = 0;

    part->startIndex = 0;
    part->vertexOffset = 0;
    part->vertexStride = static_cast<UINT>(stride);
    part->indexFormat = DXGI_FORMAT_R16_UINT;

    // Vertex buffer
    std::map<std::string, uint16_t> m3dVertIndexMap;

    // Index Buffer
    std::vector<uint16_t> indices;
    for (auto it = begin(m3dTris); it != end(m3dTris); ++it) {
        for (int i : {0, 1, 2}) {
            uint16_t currVert = it->vertex[i];
            uint16_t currNorm = it->normal[i];
            uint16_t currTexcoord = it->texcoord[i];
            VertexPositionNormalTexture vertexData = VertexPositionNormalTexture(
                XMFLOAT3(m3dVerts[currVert].x, m3dVerts[currVert].y, m3dVerts[currVert].z),
                XMFLOAT3(m3dVerts[currNorm].x, m3dVerts[currNorm].y, m3dVerts[currNorm].z),
                XMFLOAT2(m3dTex[currTexcoord].u, 1 - m3dTex[currTexcoord].v)
            );
            std::string vertexDataKey =
                std::to_string(vertexData.position.x) +
                std::to_string(vertexData.position.y) +
                std::to_string(vertexData.position.z) +
                std::to_string(vertexData.normal.x) +
                std::to_string(vertexData.normal.y) +
                std::to_string(vertexData.normal.z) +
                std::to_string(vertexData.textureCoordinate.x) +
                std::to_string(vertexData.textureCoordinate.y);

            std::map<std::string, uint16_t>::iterator mapIt = m3dVertIndexMap.find(vertexDataKey);
            if (mapIt == m3dVertIndexMap.end()) {
                uint16_t newIndex = m3dVertIndexMap.size();
                indices.push_back(newIndex);
                vertexBuffer_.push_back(vertexData);
                m3dVertIndexMap[vertexDataKey] = newIndex;
                dxtkM3dVertexMap_[newIndex] = currVert;
                dxtkM3dNormalMap_[newIndex] = currNorm;
            }
            else {
                indices.push_back(mapIt->second);
            }
        }
    }
    part->vertexBufferSize = stride * vertexBuffer_.size();
    part->vertexCount = vertexBuffer_.size();
    part->vertexBuffer = GraphicsMemory::Get(device_).Allocate(part->vertexBufferSize);
    memcpy(part->vertexBuffer.Memory(), vertexBuffer_.data(), part->vertexBufferSize);

    part->indexBufferSize = indices.size() * 2; // Each index is stored in 2 bytes
    part->indexCount = indices.size();
    part->indexBuffer = GraphicsMemory::Get(device_).Allocate(part->indexBufferSize);
    memcpy(part->indexBuffer.Memory(), indices.data(), part->indexBufferSize);
    

    part->vbDecl = std::make_shared<ModelMeshPart::InputLayoutCollection>(VertexPositionNormalTexture::InputLayout.pInputElementDescs,
        VertexPositionNormalTexture::InputLayout.pInputElementDescs + VertexPositionNormalTexture::InputLayout.NumElements);

    mesh->opaqueMeshParts.emplace_back(part);
    dxtkModel->meshes.emplace_back(mesh);

    constexpr unsigned int maxInt = std::numeric_limits<unsigned int>::max();
    std::vector<m3db_t> m3dBones = m3dModel->getBones();
    const size_t bNum = m3dBones.size();
    std::map<unsigned int, unsigned int> siblinglessChildIndex;
    ModelBone::Collection bones;
    bones.reserve(bNum);
    auto transforms = ModelBone::MakeArray(bNum);
    unsigned int currIndex = 0;

    for (auto it = begin(m3dBones); it != end(m3dBones); ++it) {
        ModelBone bone;
        std::string currName = it->name;
        unsigned int currParent = it->parent;
        bone.name = StringToWString(it->name);
        bone.parentIndex = currParent;
        bone.childIndex = maxInt;
        bone.siblingIndex = maxInt;
        // Due to the structure of M3D, parent alwys exists
        // TODO - handle edge case of malformed M3D
        if (currParent != maxInt) {
            // Parent exists and has no child
            if (bones[currParent].childIndex == maxInt) {
                bones[currParent].childIndex = currIndex;
                siblinglessChildIndex[currParent] = currIndex;
            }
            else {
                // Parent exists and has a child -> find a free sibling
                bones[siblinglessChildIndex[currParent]].siblingIndex = currIndex;
                siblinglessChildIndex[currParent] = currIndex;
            }
        }
        bones.push_back(bone);
        XMFLOAT4X4 temp = XMFLOAT4X4(it->mat4);
        transforms[currIndex] = XMLoadFloat4x4(&temp);
        currIndex++;
    }
    std::swap(dxtkModel->bones, bones);

    // Compute inverse bind pose matrices for the model
    auto bindPose = ModelBone::MakeArray(bNum);
    dxtkModel->CopyAbsoluteBoneTransforms(bNum, transforms.get(), bindPose.get());

    auto invBoneTransforms = ModelBone::MakeArray(bNum);
    for (size_t j = 0; j < bNum; ++j)
    {
        invBoneTransforms[j] = XMMatrixInverse(nullptr, bindPose[j]);
    }

    std::swap(dxtkModel->boneMatrices, transforms);
    std::swap(dxtkModel->invBindPoseMatrices, invBoneTransforms);

    dxtkModel->name = name_;

    return dxtkModel;
}

void M3dModel::ApplyAnimToDXTKModel(const DirectX::Model& dxtkModel)
{
    M3D::Model* m3dModel = static_cast<M3D::Model*>(m3dModel_);
    std::vector<m3dv_t> m3dVerts = m3dModel->getVertices();

    // Get the bind-pose skeleton
    std::vector<m3db_t> bindPose = m3dModel->getBones();

    // Get the animation-pose skeleton
    std::vector<m3db_t> animPose = m3dModel->getActionPose(1, animTime_);

    // Convert mesh vertices from bind pose to animation pose
    std::vector<VertexPositionNormalTexture> vbo = vertexBuffer_;
    std::vector<m3ds_t> m3dSkin = m3dModel->getSkin();
    int count = 0;
    for (auto it = begin(vbo); it != end(vbo); ++it) {
        m3dv_t currVert = m3dVerts[dxtkM3dVertexMap_[count]];
        m3dv_t currNorm = m3dVerts[dxtkM3dNormalMap_[count]];
        m3ds_t currSkin = m3dSkin[currVert.skinid];
        if (currVert.skinid != -1U) {
            for (int i = 0; i < M3D_NUMBONE && currSkin.weight[i] > 0.0; i++) {
                XMFLOAT4X4 bindPoseMatrixInit = XMFLOAT4X4(bindPose[currSkin.boneid[i]].mat4);
                XMFLOAT4X4 animPoseMatrixInit = XMFLOAT4X4(animPose[currSkin.boneid[i]].mat4);
                // XMFLOAT4X4 is row-major, whereas out matrices are column-major -> transpose required
                // https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmloadfloat4x4
                XMMATRIX bindPoseMatrix = XMMatrixTranspose(XMLoadFloat4x4(&bindPoseMatrixInit));
                XMMATRIX animPoseMatrix = XMMatrixTranspose(XMLoadFloat4x4(&animPoseMatrixInit));

                // POSITION
                XMFLOAT4 currVertInit = XMFLOAT4(currVert.x, currVert.y, currVert.z, 1.0f);
                XMFLOAT4 currVertUpdated;
                // Convert bind-pose from model-space to bone-local space
                XMVECTOR vertBindPoseBoneSpace = XMVector4Transform(XMLoadFloat4(&currVertInit), bindPoseMatrix);
                // Then convert from bone-local space into animation-pose model-space
                XMStoreFloat4(&currVertUpdated, XMVector4Transform(vertBindPoseBoneSpace, animPoseMatrix));
                // Multiply with weight and accumulate
                it->position.x += currVertUpdated.x * currSkin.weight[i];
                it->position.y += currVertUpdated.y * currSkin.weight[i];
                it->position.z += currVertUpdated.z * currSkin.weight[i];

                // NORMAL
                XMFLOAT4 currNormInit = XMFLOAT4(currNorm.x, currNorm.y, currNorm.z, 1.0f);
                XMFLOAT4 currNormUpdated;
                // Convert bind-pose from model-space to bone-local space
                XMVECTOR normBindPoseBoneSpace = XMVector4Transform(XMLoadFloat4(&currNormInit), bindPoseMatrix);
                // Then convert from bone-local space into animation-pose model-space
                XMStoreFloat4(&currNormUpdated, XMVector4Transform(vertBindPoseBoneSpace, animPoseMatrix));
                // Multiply with weight and accumulate
                it->normal.x += currNormUpdated.x * currSkin.weight[i];
                it->normal.y += currNormUpdated.y * currSkin.weight[i];
                it->normal.z += currNormUpdated.z * currSkin.weight[i];
            }
        }
        count++;
    }

    ModelMeshPart* currPart = dxtkModel.meshes[0].get()->opaqueMeshParts[0].get();
    memcpy(currPart->vertexBuffer.Memory(), vbo.data(), currPart->vertexBufferSize);

}

void M3dModel::UpdateAnimTime(float delta)
{
    animTime_ += static_cast<double>(delta);
}