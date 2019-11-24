# UE4ShaderPluginSandbox

English

Various UE4 shader samples made for my private projects.
Recently, there is two samples.
RenderTextureDemo and DeformGridDemo which are in Content directory of ShaderSandbox plugin.

Japanese

個人的な活動用に作ったUE4シェーダのサンプルが入っています。
現在はRenderTextureDemoとDeformGridDemo2つのサンプルがあります。
どちらもShaderSandboxプラグインのContentディレクトリの中に入っています。

## RenderTextureDemo
It has been tested only on UE4.22 and 4.23, Win64 DX11 and DX12.
On 4.22, it is necessary to copy CanvasRenderTarget2D.cpp of UE4.23 to UE4.22 and build UE4.22.

English

If you play on the test map, the render target in that folder become red by a vertex shader and a pixel shader.
And the canvas render target in that folder become green by a compute shader.

Japanese

テスト用マップをプレーすると、同フォルダにあるレンダーターゲットがVertexShaderとPixelShaderによって描画され赤一色になります。
また、キャンバスレンダーターゲットがComputeShaderによって描画され緑一色になります。

## DeformGridDemo
It has been tested only on UE4.23, Win64 DX11 and DX12, not on ray tracing.

![DeformGridDemo](Plugins/ShaderSandbox/SinWaveDeformGridMesh.gif "DeformGridDemo")

English

If you play on the test map, a DeformMeshActor begin to wave.
Vertex Buffer is waved by compute shaders.

Japanese

テスト用マップをプレーすると、DeformMeshActorがサインカーブで波打ちます。
頂点バッファをコンピュートシェーダで動かしています。
