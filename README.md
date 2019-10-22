# UE4ShaderPluginSandbox

It has tested only on UE4.23.

English

It includes ShaderSandbox plugin, which is a Hello World like plugin, which draw render target by Global Shaders.
The ShaderPluginDemo project do nothing now.
There is a test map in Content folder in ShaderSandbox plugin.
If you play on the test map, the render target in that folder become red by a vertex shader and a pixel shader.
And the canvas render target in that folder become green by a compute shader.

Japanese

レンダーターゲットをGlobal Shader Pluginを使って描画するHello World的なプラグイン、ShaderSandboxを含んでいます。
ShaderPluginDemoプロジェクトは今のところ何もしていません。
ShaderSandboxのContentフォルダにテスト用マップが入っています。
テスト用マップをプレーすると、同フォルダにあるレンダーターゲットがVertexShaderとPixelShaderによって描画され赤一色になります。
また、キャンバスレンダーターゲットがComputeShaderによって描画され緑一色になります。
