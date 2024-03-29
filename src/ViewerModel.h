#pragma once

#include <Effects.h>

#include "additionnal-dx-deps/StepTimer.h"
#include "additionnal-dx-deps/DeviceResources.h"
#include "M3dModel.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

class ViewerModel {

public:
    
    ViewerModel() = default;
    ViewerModel(const wchar_t* m3dPath);
    void Update(DX::StepTimer const& timer);
    void Render(ID3D12GraphicsCommandList* commandList, Matrix world, Matrix view, Matrix proj);
    void CreateDeviceDependentResources(ID3D12Device* device, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, ID3D12CommandQueue* commandQueue);
    void OnDeviceLost();
    
	std::wstring GetModelName()                     const   { return m3dModel_.GetName(); };
    std::vector<std::wstring> GetAnimationNames()   const   { return m3dModel_.GetAnimNames(); };
    void SetAnimation(int idx) { m3dModel_.SetAnimIdx(idx); };
    
private:
    
    const wchar_t* m3dPath_;
    M3dModel m3dModel_;
    std::unique_ptr<CommonStates> dxtkStates_;
    std::unique_ptr<DirectX::Model> dxtkModel_;
    DirectX::Model::EffectCollection dxtkModelNormal_;
    std::unique_ptr<DirectX::EffectTextureFactory> dxtkModelResources_;
    std::unique_ptr<DirectX::EffectFactory> dxtkFxFactory_;
    std::unique_ptr<BasicEffect> dxtkBasic;
};