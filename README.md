# ZMK Module: エンコーダでマウスホイールを動かす

ロータリーエンコーダをマウスホイールとして使えるようにするZMKモジュールです。  
ZMK v0.4で動作確認しています。

## 概要

通常、ZMKではエンコーダの挙動を `sensor-bindings` にて `&inc_dec_kp` として設定しますが、
これではホイールスクロールを割り当てることができません。このモジュールはその問題を解決します。

また、エンコーダの処理を press → release の2イベントではなく、
**set → 送信 → クリア → 再送信** までを1回のプロセスで完結させているため、
処理が重なってキューが溢れても release の処理を漏らすことがありません。

---

## 導入方法

### 1. west.yml の編集

> [!WARNING]
> YMLファイルはインデント（スペースの数）を間違えるだけでエラーになります。慎重に編集してください。

`remotes` セクションに追記：

```yaml
- name: abababababababa
  url-base: https://github.com/abababababababa
```

`projects` セクションに追記：

```yaml
- name: zmk_encoder
  remote: abababababababa
  revision: main
  path: modules/zmk_encoder
```

---

### 2. keymap ファイルの編集

ファイルの先頭でインクルードを追加：

```c
#include <dt-bindings/zmk/pointing.h>
```

ビヘイビアを定義します。エンコーダが2個ある場合の例です。
1個だけの場合は片方のみ記述してください。

```dts
/ {
    behaviors {
        ec_ms_l: ec_ms_h {
            compatible = "zmk,behavior-encoder-mouse-scroll";
            #sensor-binding-cells = <2>;
        };
        ec_ms_r: ec_ms_v {
            compatible = "zmk,behavior-encoder-mouse-scroll";
            #sensor-binding-cells = <2>;
        };
    };
};
```

レイヤー内でエンコーダの動作を割り当てます：

```dts
sensor-bindings = <&ec_ms_l SCRL_LEFT SCRL_RIGHT &ec_ms_r SCRL_DOWN SCRL_UP>;
```

| バインディング | 時計回り | 反時計回り |
|---|---|---|
| `&ec_ms_l` | `SCRL_LEFT` | `SCRL_RIGHT` |
| `&ec_ms_r` | `SCRL_DOWN` | `SCRL_UP` |

使用できる方向は `SCRL_UP` `SCRL_DOWN` `SCRL_LEFT` `SCRL_RIGHT` の4種類です。
時計回りと反時計回りに異なる方向を組み合わせることもできます。