add_library(OpenGLESRHI
        OpenGLESGraphicsManager.cpp
        ${PROJECT_SOURCE_DIR}/External/GLES/src/glad_egl.c
)
target_include_directories(OpenGLESRHI PRIVATE "${PROJECT_SOURCE_DIR}/External/GLES/include")