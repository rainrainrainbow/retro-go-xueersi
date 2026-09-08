set(COMPONENT_SRCDIRS ".")
set(COMPONENT_ADD_INCLUDEDIRS ".")
register_component()
# 第三方解码库代码质量参差，禁用将警告视为错误以避免 xtensa 编译器细微警告导致失败
rg_setup_compile_options(-O2 -Wno-error=unused-variable -Wno-error=unused-function -Wno-error=sign-compare -Wno-error=type-limits -Wno-error=maybe-uninitialized -Wno-error=implicit-fallthrough)
