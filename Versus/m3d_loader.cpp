#include "pch.h"
#include "m3d_loader.h"
#include "m3d.h"


std::unique_ptr<Model> __cdecl M3DLoader::CreateFromM3D(ID3D12Device* device, const wchar_t* szFileName, ModelLoaderFlags flags)
{
	return std::unique_ptr<DirectX::Model>();
}
