# PSoC5 OpenJTAG Adapter

[PSoC 5LP Prototyping Kit (CY8CKIT-059)](http://japan.cypress.com/?rid=108038)用のOpenJTAG実装です。

- P12.0 --> TDI
- P12.1 <-- TDO
- P12.2 --> TCK
- P12.3 --> TMS
- P12.4 --- (not used)
- P12.5 --> TRST (target reset signal, active low.)
- P1.7  <-- VTref (target voltage reference.)

OpenOCDを実行するデバッグホストに、PSoC 5LP Prototyping KitのMicroUSBを接続します。

KitProgのCOMポートを介して監視・制御することができます。

TRSTはオプションのハードリセット信号です。

TDI/TCK/TMSの出力電圧は、外部リファレンスモードと内部3.3Vモードから選択できます。電源投入直後は、外部リファレンスモードです。外部リファレンスモードでは、ターゲットのVCCに接続したVTrefの電圧を、出力電圧として使用します。KitProgのCOMポートから'i'を送信することで、内部3.3Vモードに切り替えることができます。'e’を送信すると、外部リファレンスモードに戻ります。
