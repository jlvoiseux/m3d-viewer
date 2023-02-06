//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "ViewerModel.h"
#include "StepTimer.h"

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Viewer final : public DX::IDeviceNotify
{
public:

    Viewer() noexcept(false);
    ~Viewer();

    Viewer(Viewer&&) = default;
    Viewer& operator= (Viewer&&) = default;

    Viewer(Viewer const&) = delete;
    Viewer& operator= (Viewer const&) = delete;

    // Initialization and management
    std::unique_ptr<DX::DeviceResources>& Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;

    DirectX::SimpleMath::Matrix m_world;
    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_proj;
    
    ViewerModel pc;
};
