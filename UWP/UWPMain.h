#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "LCUIInput.h"
#include "LCUIRenderer.h"

// 在屏幕上呈现 Direct2D 和 3D 内容。
namespace UWP
{
	class UWPMain : public DX::IDeviceNotify
	{
	public:
		UWPMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~UWPMain();
		void CreateWindowSizeDependentResources();
		void Update();
		bool Render();

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		// 缓存的设备资源指针。
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<LCUIRenderer> m_renderer;
		// 渲染循环计时器。
		DX::StepTimer m_timer;
	};
}
