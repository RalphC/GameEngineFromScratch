#pragma once

#include "IApplication.hpp"
#include "GfxConfiguration.h"

namespace My
{
	class BaseApplication : implements IApplication
	{
	public:
		BaseApplication(GfxConfiguration& cfg);
		virtual int Initialize();
		virtual void Finalize();
		virtual void Tick();
		virtual bool IsQuit();

		inline GfxConfiguration& GetConfiguration() { return m_Config; };

	protected:
		static bool m_bQuit;
		GfxConfiguration m_Config;

	private:
		BaseApplication() {};
	};
}
