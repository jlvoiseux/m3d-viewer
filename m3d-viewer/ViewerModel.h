#pragma once
#include "M3dModel.h"
#include "StepTimer.h"
#include "DeviceResources.h"
#include "SimpleMath.h"
#include <Effects.h>

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