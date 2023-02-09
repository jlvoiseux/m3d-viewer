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
	// If there are no texture, just display vertex colors
    if (dxtkModel_->textureNames.empty()) 
    {
        dxtkBasic->SetWorld(world);
        dxtkBasic->SetView(view);
        dxtkBasic->SetProjection(proj);
        dxtkBasic->Apply(commandList);
        dxtkModel_->Draw(commandList);
    }
    else 
    {
        ID3D12DescriptorHeap* heaps[] = { dxtkModelResources_->Heap(), dxtkStates_->Heap() };
        commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);
        Model::UpdateEffectMatrices(dxtkModelNormal_, world, view, proj);
        dxtkModel_->Draw(commandList, dxtkModelNormal_.cbegin());
    }

	if (m3dModel_.GetAnimNames().size() > 0)
    {
        m3dModel_.ApplyAnimToDXTKModel(*dxtkModel_);
    }
}

void ViewerModel::CreateDeviceDependentResources(ID3D12Device* device, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, ID3D12CommandQueue* commandQueue)
{
    const auto& cull = CommonStates::CullClockwise;
    dxtkStates_ = std::make_unique<CommonStates>(device);
    ResourceUploadBatch resourceUpload(device);
    RenderTargetState rtState(backBufferFormat, depthBufferFormat);

    m3dModel_ = M3dModel(device, m3dPath_);
    dxtkModel_ = m3dModel_.BuildDXTKModel();
    if (!dxtkModel_->textureNames.empty())
    {
        EffectPipelineStateDescription pd(nullptr, CommonStates::Opaque, CommonStates::DepthDefault, CommonStates::CullClockwise, rtState);
        resourceUpload.Begin();
        dxtkModelResources_ = dxtkModel_->LoadTextures(device, resourceUpload);
        dxtkFxFactory_ = std::make_unique<EffectFactory>(dxtkModelResources_->Heap(), dxtkStates_->Heap());
        auto uploadResourcesFinished = resourceUpload.End(commandQueue);
        uploadResourcesFinished.wait();
        dxtkModelNormal_ = dxtkModel_->CreateEffects(*dxtkFxFactory_, pd, pd);
    }
    else {
        EffectPipelineStateDescription pd(&VertexPositionNormalColorTexture::InputLayout, CommonStates::Opaque, CommonStates::DepthDefault, CommonStates::CullClockwise, rtState);
        dxtkBasic = std::make_unique<BasicEffect>(device, EffectFlags::Lighting|EffectFlags::VertexColor, pd);
    }
}

void ViewerModel::OnDeviceLost()
{
    dxtkStates_.reset();
	dxtkFxFactory_.reset();
	dxtkModelResources_.reset();
	dxtkModel_.reset();
	dxtkModelNormal_.clear();
	dxtkBasic.reset();
}
