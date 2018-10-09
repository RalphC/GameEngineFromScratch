#pragma once
#include "GraphicsManager.hpp"

namespace My {
    class OpenGLESGraphicsManager : public GraphicsManager
    {
    public:
        virtual int Initialize();
        virtual void Finalize();

        virtual void Tick();
    private:
    };
}