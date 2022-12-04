#pragma once
#include <memory>
#include "Model.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class M3dModel {

public:
    M3dModel() = default;
    M3dModel(ID3D12Device* device, const wchar_t* szFileName);
    std::unique_ptr<Model> BuildDXTKModel();
    void UpdateAnimTime(float elapsedTime);
    void ApplyAnimToDXTKModel(const DirectX::Model& dxtkModel);
private:
    ID3D12Device* device_;
    void* m3dModel_;
    const wchar_t* name_;
    float animTime_;
    std::vector<VertexPositionNormalTexture> vertexBuffer_;
    std::map<uint16_t, uint16_t> m3dDxtkVertexMap_;
    std::map<uint16_t, uint16_t> m3dDxtkNormalMap_;
};


