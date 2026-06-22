# SimpleCQ

[English](README.md) | [中文](README_zh.md) | [한국어](README_ko.md) | [日本語](README_ja.md)

## 概要

SimpleCQ は、コンピュータ生成戦力（CGF）の行動学習を対象とした、深層強化学習（DRL）向けのオープンソースシミュレーションフレームワークです。

SimpleCQ は、設定駆動の CGF シナリオ実行を維持しながら、学習ワークロードに必要な実行インターフェースを提供します。異種モデル DLL を読み込み、YAML で定義されたトピック規則に従ってエンティティ出力をルーティングし、再利用可能なタスクグラフでシミュレーションフレームを進め、エージェントモデルを通じて Python 側の強化学習ランナーと接続します。

この README では、特にエンジンと加速機能を重視しています。

- Taskflow ベースのフレームスケジューリングにより、固定的な直列モデルループを置き換えます。
- トピックルーティングにより、モデル出力、メッセージ変換、対象入力を分離します。
- ダブルバッファ方式のトピック交換により、決定論的なフレーム意味を保ちます。
- 動的モデルの生成と削除をフレームタスクグラフに統合します。
- 非同期 rollout worker と Gym 風エージェントインターフェースにより、高スループットの DRL サンプリングをサポートできます。
- 交替 self-play runner により、敵対的な方策学習をサポートできます。
- `output_movable` は、所有権移動が安全なモデルで移動ベースの出力転送を可能にします。

## ランタイム構成

エンジンは各フレームを次の因果順序で進めます。

1. 各モデルが `GetOutput` で出力を公開します。
2. トピック規則が必要なメンバーの有無を確認します。
3. トピックデータが変換され、購読者向けに収集されます。
4. トピックバッファを交換し、すべてのモデルが同一フレームに整列した入力を読みます。
5. 対象モデルが `SetInput` でデータを受け取ります。
6. 各モデルが `Tick(dt)` で状態を進めます。
7. コールバックで要求された動的モデルを生成し、破棄済みモデルを有効期限後に削除します。

この流れは `src/engine/executionengine.hpp` に実装されています。`ExecutionEngine::buildGraph()` はフレームを Taskflow DAG にコンパイルし、`ExecutionEngine::run(times)` はその DAG を再利用して複数フレームを進め、FPS を記録します。

## リポジトリ構成

| パス | 目的 |
| --- | --- |
| `src/engine/` | ランタイムエンジン、コンソールインターフェース、タスクグラフスケジューリング、モデル管理 |
| `src/model.cpp` | `assemble.yml` により複数の原子モデルを結合する組み立てモデル DLL |
| `src/agent.cpp` | Python 側学習相互作用のための socket ベースのエージェントモデル DLL |
| `src/gym_interface.py` | Python 側 Gym 風通信ヘルパー |
| `config/` | サンプルシナリオ、組み立てモデル設定、モデル記述ファイル |
| `scripts/` | マルチプロセス、ベクトル環境、self-play 学習 notebook |
| `3rd-party/` | 同梱された Taskflow、yaml-cpp、RapidXML ソース |
| `agent/` | 任意利用の文書化および設定確認アシスタント |
| `doc/README_Detailed.md` | 詳細なエンジン説明と拡張利用ノート |

## ビルド

推奨環境:

- Windows と MSVC 2022
- `CMakePresets.json` を使う場合は CMake 3.21 以上
- Ninja
- `./vcpkg` に配置された vcpkg、または `CMakePresets.json` の `toolchainFile` 修正

vcpkg で宣言されている依存関係は `mimalloc` のみです。Taskflow と yaml-cpp は `3rd-party/` に同梱されています。

```powershell
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-vcpkg-release
```

主な生成物は `bin/Release/` に出力されます。

- `tinycq.exe`: 対話型シミュレーションコントローラ
- `mymodel.dll`: 組み立てモデルラッパー
- `agent.dll`: 学習用 socket ベースエージェントモデル

GitHub Actions ワークフロー `.github/workflows/compile.yml` は、`windows-latest` 上で同じ CMake preset を使用します。

## クイックスタート

サンプルを実行する前に、選択した YAML ファイル内の `dll_path` を更新してください。`config/` 内の一部ファイルには、元の実験環境の絶対パスが残っています。

```powershell
.\bin\Release\tinycq.exe
```

コンソール内:

```text
>>> load config\navigation.yml
>>> set dt 100
>>> set drawrate 100
>>> r 1000
>>> model
```

主なコマンド:

| コマンド | 意味 |
| --- | --- |
| `load <file>` または `l <file>` | シナリオ YAML を読み込む |
| `run <frames>` または `r <frames>` | シミュレーションを指定フレーム数だけ進める |
| `cfg` | 実行時設定を一覧表示する |
| `get <key>` | 実行時設定を確認する |
| `set <key> <value>` | 実行時設定を変更する |
| `print` または `p` | 現在のトピックバッファを出力する |
| `model` | 静的モデルと動的モデルを一覧表示する |

## 詳細なエンジン説明

エンジン機構、トピックスケジューリング、組み立てモデルの再起動、シナリオルーティング規則、プロファイリング、可視化、ソース構成に関する詳しい説明は [doc/README_Detailed.md](doc/README_Detailed.md) を参照してください。

## シナリオ設定

シナリオ YAML ファイルは、実行可能なシミュレーショングラフを記述します。

- `model_types`: モデル型名、DLL パス、`output_movable` 設定。
- `models`: `model_type`、`side_id`、`id`、XML 形式の `init_value` を持つモデルインスタンス。
- `topics`: publish-subscribe 規則。公開者、必要な出力メンバー、購読者、名前変換規則を含みます。

協調 UAV 偵察の例は `config/navigation.yml` に、2-vs-2 敵対 self-play の例は `config/scene_planebattle2v2_self_learning.yml` に対応します。

## 組み立てモデル設定

`mymodel.dll` は DLL ディレクトリから `assemble.yml` を読み込みます。組み立てモデルは、複数の原子モデルを 1 つの CGF モデルとして公開します。設定ファイルは次の要素を含みます。

- `config`: 任意のプロファイル出力先、再起動キー、陣営フィルタ、ログレベル。
- `models`: 原子モデル DLL と論理名。
- `init_convert`: ルート初期化データをサブモデルへルーティングします。
- `input_convert`: ルート入力データをサブモデルへルーティングします。
- `output_convert`: サブモデル出力をルートまたは他のサブモデルへルーティングします。

`restart_key` が設定され、`SetInput` データにそのキーが含まれる場合、組み立てモデルは次の `GetOutput` の前にサブモデルを再生成して初期化します。

## DRL インターフェース

`src/agent.cpp` のエージェントモデルは `127.0.0.1:<port>` に接続し、シリアライズされた `CSValueMap` データを Python と交換します。これはシミュレータ側の `Init`、`SetInput`、`GetOutput`、`Tick` を学習側の reset-step ループとして扱います。

- `reset` はシナリオを初期化し、整列済み観測を返します。
- `step(action)` はコマンドをトピックデータに書き込み、フレームを進め、観測、報酬、終了フラグを返します。
- 学習スクリプトは複数環境と非同期 rollout worker をサポートし、サンプルスループットを高められます。

## 例

次の例は、現在サポート可能なワークフローを示します。

- 協調 UAV 偵察: `config/navigation.yml`, `scripts/Train_vecenv.ipynb`.
- 非同期およびマルチプロセス rollout 加速をサポートできます: `scripts/Train_Multiprocess.ipynb`.
- 2-vs-2 交替 self-play をサポートできます: `config/scene_planebattle2v2_self_learning.yml`, `scripts/Train2v2_self_learning.ipynb`.

## ライセンス

本プロジェクトは MIT License の下で公開されています。詳細は `LICENSE` を参照してください。
