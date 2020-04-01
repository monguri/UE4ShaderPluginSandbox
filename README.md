# UE4ShaderPluginSandbox

English

Various UE4 shader samples made for my private projects.
Recently, there is three samples.
RenderTextureDemo, SinWaveGridDemo and ClothGridDemo which are in Content directory of ShaderSandbox plugin.
It has been confirmed that it can be build on Visual Studio 2017 Win64.

Japanese

個人的な活動用に作ったUE4シェーダのサンプルが入っています。
現在はRenderTextureDemoとSinWaveGridDemoとClothGridDemoの3つのサンプルがあります。
どちらもShaderSandboxプラグインのContentディレクトリの中に入っています。
VisualStudio2017、Win64でビルドできることを確認しています。

## RenderTextureDemo
It has been tested only on UE4.22, 4.23 and 4.24, Win64 DX11 and DX12.
On 4.22, it is necessary to copy CanvasRenderTarget2D.cpp of UE4.23 to UE4.22 and build UE4.22.

English

If you play on the test map, the render target in that folder become red by a vertex shader and a pixel shader.
And the canvas render target in that folder become green by a compute shader.

Japanese

テスト用マップをプレーすると、同フォルダにあるレンダーターゲットがVertexShaderとPixelShaderによって描画され赤一色になります。
また、キャンバスレンダーターゲットがComputeShaderによって描画され緑一色になります。

## SinWaveGridDemo
It has been tested only on UE4.23 and 4.24, Win64 DX11 and DX12, not on ray tracing.

![SinWaveGridDemo](Plugins/ShaderSandbox/SinWaveDeformGridMesh.gif "SinWaveGridDemo")

English

If you play on the test map, a SinWaveMeshActor begin to wave.
Vertex Buffer is waved by compute shaders.

Japanese

テスト用マップをプレーすると、SinWaveMeshActorがサインカーブで波打ちます。
頂点バッファをコンピュートシェーダで動かしています。

## ClothGridDemo
It has been tested only on UE4.23 and 4.24, Win64 DX11, not on ray tracing.
DX12 will be tested soon.

![ClothGridDemo](Plugins/ShaderSandbox/ClothWind.gif "SinWaveGridDemo")

English

If you play on the test map, several ClothGridActor begin to move.
Cloth siumlation is done by compute shaders.

Japanese

テスト用マップをプレーすると、いくつかのClothGridActorが動きます。
クロスシミュレーションはコンピュートシェーダで行っています。

## OceanDemo
It has been tested only on 4.24, Win64 DX11, not on ray tracing.
DX12 will be tested soon.

![OceanDemo](Plugins/ShaderSandbox/OceanIslandShort.gif "OceanDemo")

English

If you play on the test map, ocean surface simulation starts.

Japanese

テスト用マップをプレーすると、海面のシミュレーションがはじまります。
