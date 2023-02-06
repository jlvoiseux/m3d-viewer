#include "pch.h"
#include "ViewerModel.h"

ViewerModel::ViewerModel(const wchar_t* m3dPath)
{
	m3dPath_ = m3dPath;
}

void ViewerModel::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());
    m3dModel_.UpdateAnimTime(elapsedTime * 1000);
}

void ViewerModel::Render(ID3D12GraphicsCommandList* commandList, Matrix world, Matrix view, Matrix proj)
{
    ID3D12DescriptorHeap* heaps[] = { dxtkModelResources_->Heap(), dxtkStates_->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    Model::UpdateEffectMatrices(dxtkModelNormal_, world, view, proj);

    m3dModel_.ApplyAnimToDXTKModel(*dxtkModel_);
    dxtkModel_->Draw(commandList, dxtkModelNormal_.cbegin());
}

void ViewerModel::CreateDeviceDependentResources(ID3D12Device* device, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, ID3D12CommandQueue* commandQueue)
{
    const auto& cull = CommonStates::CullClockwise;
    dxtkStates_ = std::make_unique<CommonStates>(device);
    ResourceUploadBatch resourceUpload(device);
    RenderTargetState rtState(backBufferFormat, depthBufferFormat);
    EffectPipelineStateDescription pd(nullptr, CommonStates::Opaque, CommonStates::DepthDefault, CommonStates::CullClockwise, rtState);

    m3dModel_ = M3dModel(device, L"mc.m3d");
    dxtkModel_ = m3dModel_.BuildDXTKModel();

    resourceUpload.Begin();
    //m_model->LoadStaticBuffers(device, resourceUpload, true);
    dxtkModelResources_ = dxtkModel_->LoadTextures(device, resourceUpload);
    dxtkFxFactory_ = std::make_unique<EffectFactory>(dxtkModelResources_->Heap(), dxtkStates_->Heap());
    auto uploadResourcesFinished = resourceUpload.End(commandQueue);
    uploadResourcesFinished.wait();

    dxtkModelNormal_ = dxtkModel_->CreateEffects(*dxtkFxFactory_, pd, pd);
}

void ViewerModel::OnDeviceLost()
{
    dxtkStates_.reset();
	dxtkFxFactory_.reset();
	dxtkModelResources_.reset();
	dxtkModel_.reset();
	dxtkModelNormal_.clear();
}
