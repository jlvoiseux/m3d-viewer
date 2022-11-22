#pragma once
#include <Model.h>
#include <memory>
using namespace DirectX;

class M3DLoader
{
public:
    static std::unique_ptr<Model> __cdecl CreateFromM3D(
        _In_opt_ ID3D12Device* device,
        _In_z_ const wchar_t* szFileName,
        ModelLoaderFlags flags = ModelLoader_Default);
};

