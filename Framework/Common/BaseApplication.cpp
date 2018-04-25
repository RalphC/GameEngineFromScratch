#include <iostream>
#include "BaseApplication.hpp"

bool My::BaseApplication::m_bQuit = false;

My::BaseApplication::BaseApplication(GfxConfiguration& cfg) : m_config(cfg)
{

}

int My::BaseApplication::Initialize()
{
	m_bQuit = false;

	return 0;
}

void My::BaseApplication::Finalize()
{

}

void My::BaseApplication::Tick()
{

}

bool My::BaseApplication::IsQuit()
{
	std::cout << m_config;
	return 0;
}