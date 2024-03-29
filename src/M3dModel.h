#pragma once
#include <memory>
#include <iostream>
#include <string>
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
    
    std::wstring GetName()                      const   { return name_; };
	std::vector<std::wstring> GetAnimNames()    const   { return animNames_; }
	void SetAnimIdx(int idx)                            { animIdx_ = idx; animTime_ = 0;}
    
private:
    
    ID3D12Device* device_;
    void* m3dModel_;
    std::wstring name_;
	std::wstring containing_dir_;
    float animTime_;
    int animIdx_;
	std::vector<std::wstring> animNames_;
    std::vector<VertexPositionNormalColorTexture> vertexBuffer_;
    std::map<uint16_t, uint16_t> dxtkM3dVertexMap_;
    std::map<uint16_t, uint16_t> dxtkM3dNormalMap_;
};


