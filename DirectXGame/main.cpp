#include "KamataEngine.h"
#include "Shader.h"

#include <DirectXMath.h>
#include <Windows.h>

using namespace KamataEngine;
using namespace DirectX;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// エンジンの初期化
	KamataEngine::Initialize(L"LE3C_02_ウチボリ_ユウタ");

	// DirectXCommonインスタンスの取得
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// DirectXCommonクラスが管理している、ウィンドウの幅と高さの値の取得
	int32_t w = dxCommon->GetBackBufferWidth();
	int32_t h = dxCommon->GetBackBufferHeight();
	DebugText::GetInstance()->ConsolePrintf(std::format("width: {}, height: {}\n", w, h).c_str());

	// DirectXCommonクラスが管理している、コマンドリストの取得
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// RootSignature作成 -------------------------------
	// 構造体にデータを用意する
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		DebugText::GetInstance()->ConsolePrintf(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// ルートシグネチャ生成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout -------------------------------
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState -------------------------------
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState -------------------------------
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面(反時計回り)をカリングする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 塗りつぶしモードをソリッドにする(ワイヤーフレームなら D3D12_FILL_MODE_WIREFRAME)
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	// 頂点シェーダーの読み込みとコンパイル
	Shader vs;
	vs.LoadDxc(L"Resources/shaders/TestVS.hlsl", L"vs_6_0");
	assert(vs.GetDxcBlob() != nullptr);

	// ピクセルシェーダーの読み込みとコンパイル
	Shader ps;
	ps.LoadDxc(L"Resources/shaders/TestPS.hlsl",L"ps_6_0");
	assert(ps.GetDxcBlob() != nullptr);


	// PSO(PipelineStateObject)の生成 -------------------------------
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;                             // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;                              // InputLayout
	graphicsPipelineStateDesc.VS = {vs.GetDxcBlob()->GetBufferPointer(), vs.GetDxcBlob()->GetBufferSize()}; // VertexShader
	graphicsPipelineStateDesc.PS = {ps.GetDxcBlob()->GetBufferPointer(), ps.GetDxcBlob()->GetBufferSize()}; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;                                     // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;                           // RasterizerState

	// DepthStencilState追加 -----------------------
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;   // 深度テストを無効化
	depthStencilDesc.StencilEnable = FALSE; // ステンシルテストを無効化
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1; // 1つのRTVに書き込む 複数同時にしようと思えばできる
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ(形状)のタイプ 三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定(今は気にしなくていい)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成 PSO生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// VertexResourceの生成 ----------
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // CPUから書き込むヒープ
	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
	vertexResourceDesc.Width = sizeof(Vector4) * 3;                 // リソースのサイズ。今回は Vector4を3頂点分
	// バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// 実際に頂点リソースを生成する
	ID3D12Resource* vertexResource = nullptr;
	hr = dxCommon->GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr)); // うまくいかなかったときは起動できない

	// 頂点バッファへのデータ転送 ----------
	// 頂点データ
	Vector4 vertices[3] = {
		{-0.5f, -0.5f, 0.0f, 1.0f}, // 左下
		{0.0f,  0.5f,  0.0f, 1.0f}, // 上
		{0.5f,  -0.5f, 0.0f, 1.0f}  // 右下
	};

	// 頂点バッファへのデータ転送
	Vector4* vertexData = nullptr;
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// 頂点データをコピー
	std::memcpy(vertexData, vertices, sizeof(vertices));
	// 書き込み終了
	vertexResource->Unmap(0, nullptr);

	// VertexBufferViewを作成する ----------
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭アドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つのサイズ
	vertexBufferView.SizeInBytes = sizeof(XMFLOAT4) * 3;
	// 1つの頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(XMFLOAT4);

	// メインループ
	while (true) {
		// エンジンの更新
		if (KamataEngine::Update()) {
			break;
		}

		// 描画開始
		dxCommon->PreDraw();

		// ビューポートとシザー矩形の設定 (追加)
		D3D12_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(w);
		viewport.Height = static_cast<float>(h);
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);

		D3D12_RECT scissorRect{};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = w;
		scissorRect.bottom = h;
		commandList->RSSetScissorRects(1, &scissorRect);

		// コマンドを積む
		// RootSignatureの設定
		commandList->SetGraphicsRootSignature(rootSignature);
		// PipelineStateの設定
		commandList->SetPipelineState(graphicsPipelineState);
		// VBVの設定する
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		// トポロジの設定
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 頂点数、インデックス数、インデックスの開始位置、インデックスのオフセット
		commandList->DrawInstanced(3, 1, 0, 0);

		// 描画終了
		dxCommon->PostDraw();
	}

	// 解放処理
	vertexResource->Release();
	graphicsPipelineState->Release();
	signatureBlob->Release();
	//if (shaderErrorBlob) { // 追加: シェーダーエラーブロブの解放
	//	shaderErrorBlob->Release();
	//}
	rootSignature->Release();

	// エンジンの終了処理
	KamataEngine::Finalize();

	return 0;
}

//ID3DBlob* CompileShader(const std::wstring& filePath, const std::string& shaderModel) {
//	ID3DBlob* shaderBlob = nullptr;
//	ID3DBlob* errorBlob = nullptr;
//
//
//	HRESULT hr = D3DCompileFromFile(
//		filePath.c_str(), // シェーダーファイル名
//		nullptr,
//		D3D_COMPILE_STANDARD_FILE_INCLUDE,
//		"main", shaderModel.c_str(),
//		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
//		0, &shaderBlob, &errorBlob);
//
//	if (FAILED(hr)) {
//		if (errorBlob) {
//			OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
//			errorBlob->Release();
//		}
//		assert(false);
//	}
//
//	return shaderBlob;
//}