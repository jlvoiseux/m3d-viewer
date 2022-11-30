#pragma once
#include <Model.h>
#include <memory>
using namespace DirectX;

class ModelExtended : public Model
{
public:

    static std::unique_ptr<Model> __cdecl CreateFromM3D(
        _In_opt_ ID3D12Device* device,
        _In_reads_bytes_(dataSize) const uint8_t* meshData, 
        _In_ size_t dataSize,
        ModelLoaderFlags flags = ModelLoader_Default);

    static std::unique_ptr<Model> __cdecl CreateFromM3D(
        _In_opt_ ID3D12Device* device,
        _In_z_ const wchar_t* szFileName,
        ModelLoaderFlags flags = ModelLoader_Default);

    static std::wstring StringToWString(std::string input);
};

