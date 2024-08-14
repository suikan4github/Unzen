# Unzen Audio framework
This is an archive of the Unzen Audio forme on mbed. 

## はじめに
mbed用のオーディオ・フレームワーク『雲仙』を開発、公開しました。 『雲仙』はI2S回線を利用したオーディオ入出力のフレームワークで、別途開発したオーディオ・コーデック・コントローラ『島原』と組み合わせることで以下のような機能を提供します。

- アナログ・オーディオを入力し、mbedで信号処理を行い、アナログ・オーディオを出力する
- 単精度浮動小数点型によるインターフェース
- サンプル周波数32kHz, 44.1kHz, 48kHz, 96kHz に対応
- サンプル・バイ・サンプル処理、ブロック処理の双方に対応
- 信号処理コールバックおよび信号処理初期化コールバックを提供
- 信号処理と並行してmbed-SDKが提供するAPIを実行可能
- mbed / mbed RTOSに対応
- UMB-ADAU1361Aと数本の線で結線することで、すぐに使うことが出来る
- 以下のmbedプラットホームに対応
    - LPCXpresso 4337
    - LPC4088 Quick Start board
    - NUCLEO-F746ZG 

以下、雲仙の利用方法について説明します。

## 信号処理を行う
信号処理は、信号処理コールバック関数の中に記述します。このコールバック関数は雲仙の Framework::start() メソッド実行時にフレームワークに登録され、以後I2S受信データが貯まる度に呼び出されます。

サンプル･アプリケーションの信号処理コールバック関数はprocess_callback()です。名前はアプリケーションによって変えてかまいません。

```C++
    // customer signal processing call back.
void process_callback(
            float rx_left_buffer[],     // array of the left input samples
            float rx_right_buffer[],    // array of the right input samples
            float tx_left_buffer[],     // place to write the left output samples
            float tx_right_buffer[],    // place to write the left output samples
            unsigned int block_size     // block size [sample]
            )
{
        // Sample processing
    for ( int i=0; i<block_size; i++)   // for all sample
    {
        tx_left_buffer[i] = rx_left_buffer[i];      // copy from input to output
        tx_right_buffer[i] = rx_right_buffer[i];
        
    }
}

```

信号処理コールバック関数は入力デーバッファ配列を2本（左右チャンネル用）、出力データバッファ配列を2本（左右チャンネル用）ブロック・サイズ指定用変数を一つ、引数として受け取ります。

ブロック･サイズ指定用変数 block_size は、一回のコールバック関数呼び出しで、何サンプルを処理すべきか知らせます。最小値は1で、この値はデフォルトでは1です。block_sizeを変更したい場合には、set_block_size() メソッドを呼び出して設定してください。ただし、satart() メソッド実行前に設定する必要があります。

block_size == 1 のとき、入出力のデータ数はそれぞれ左右1データずつとなります。

信号処理コールバック関数に与えられる信号は浮動小数点型であり、+/-1の範囲に収まっています。信号処理の結果を出力（送信）データとして与える場合には、必ず+/-の範囲に収めてください。これは信号処理プログラマの責任です。

なお、信号処理コールバックは、main()関数ではなく、割り込みコンテキストで実行されます。また、block_size > 1 の場合、処理途中で割り込まれることもあり得ます。信号処理コールバック関数の処理は、その呼び出し終期よりも短い時間で終了する必要があります。雲仙フレームワークはこの処理が所定の時間内に終わったか否かを確認しません。また、所定の時間内に終わらない場合に何が起きるかは不定です。

## 信号処理の初期化を行う

信号処理コールバック内部でフィルタを使うときなど、それらを初期化したいことがあります。初期化関数はmain()の中で自分で呼んでもかまわないのですが、初期化コールバックに記述することで目的がはっきりし、かつ正しいタイミングで呼び出すことができます。呼び出しはフレームワークが行います。

サンプル･アプリケーションの信号処理コールバック関数はinit_callback()です。名前はアプリケーションによって変えてかまいません。
```C++
    // customer signal processing initialization call back.
void init_callback(
            unsigned int block_size     // block size [sample]
            )
{
        // place initialization code here
}
```
サンプルの初期化コールバックは空ですが、ここに好きな初期化コードを書けば、そのコードはフレームワークの実行開始時に一度だけ、割り込みが発生する前に呼ばれます。

## 管理プログラムの実装例

main.cpp の main() 関数には信号処理アルゴリズムの外側で実行すべき管理プログラムを書きます。一般にはUIの処理、外部との通信、ファイル・アクセスなどをここに書きます。ここではmbed SDKが提供するAPIを使うことが出来ます。

以下の例では[Shimabara](https://github.com/suikan4github/Shimabara)コーデックドライバを使ってADAU1361Aを初期化しています。

```C++
#include "unzen.h"          // audio framework include file
#include "umb_adau1361a.h"     // audio codec contoler include file
#include "mbed.h"

#define CODEC_I2C_ADDR 0x38

DigitalOut myled1(LED1);


int main() 
{    
        // I2C is essential to talk with ADAU1361
    I2C i2c(SDA, SCL);

        // create an audio codec contoler
    shimabara::UMB_ADAU1361A codec(shimabara::Fs_32, &i2c, CODEC_I2C_ADDR ); // Default Fs is 48kHz

       // create an audio framework by singlton pattern
    unzen::Framework audio;
 
         // Set I3C clock to 100kHz
    i2c.frequency( 100000 );

        // Configure the optional block size of signal processing. By default, it is 1[Sample] 
//    audio.set_block_size(16);
    
        // Start the ADAU1361. Audio codec starts to generate the I2C signals 
    codec.start();

        // Start the audio framework on ARM processor.  
    audio.start( init_callback, process_callback);     // path the initializaiton and process call back to framework 

        // periodically changing gain for test
    while(1)     
    {
        wait(1.0);
    }
}
```

initialize_system()関数は名前の通り、システムの初期化を行います。オーディオに関連する部分はaudio変数とcodec変数にまとめられています。

```C++
void initialize_system(void)
{
        // Set I2C clock to 100kHz
    i2c.frequency( 100000 );
 
        // Configure the optional block size of signal processing. By default, it is 1[Sample] 
    audio.set_block_size(BLOCKSIZE);

        // Start UI module.
    ukifune::init( & audio );
   
        // Start the ADAU1361. Audio codec starts to generate the I2C signals 
    codec.start();
        // Start the audio framework on ARM processor.  
    audio.start( init_callback, process_callback);     // path the initializaiton and process call back to framework 
 
        // Setup initial analog gain   
    codec.set_hp_output_gain( 0, 0 );
    codec.set_line_output_gain( 0, 0 );
}
```


## ライセンス

このプログラムは[MITライセンス](LICENSE)に従って公開しています。