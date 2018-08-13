#include <iostream>
#include "BaseApplication.hpp"

using namespace My;

bool My::BaseApplication::m_bQuit = false;

My::BaseApplication::BaseApplication(GfxConfiguration& cfg) : m_Config(cfg)
{

}

int My::BaseApplication::Initialize()
{
	m_bQuit = false;
	std::cout << m_Config;
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
	std::cout << m_Config;
	return 0;
}