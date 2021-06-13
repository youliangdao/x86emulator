#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* メモリは1MB */
#define MEMORY_SIZE (1024 * 1024)

enum Register { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI, REGISTERS_COUNT };
char* registers_name [] = {
  "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"
};

//Emulator型の構造体を定義
typedef struct {
  //汎用レジスタ
  uint32_t registers[REGISTERS_COUNT];

  //EFLAGSレジスタ
  uint32_t eflags;

  /*メモリ（バイト列）
  →ここでは8ビットのデータが格納されているメモリアドレスを表している。
  　またmemoryアドレスの指すメモリ領域には1バイトごとに要素をもつ配列が定められているイメージ
  */
  uint8_t* memory;

  //プログラムカウンタ
  uint32_t eip;

} Emulator;

//エミュレータ作成の関数
Emulator* create_emu(size_t size, uint32_t eip, uint32_t esp)
{
  Emulator* emu = malloc(sizeof(Emulator));
  emu->memory = malloc(size);

  //汎用レジスタの初期値を全て0にする
  memset(emu->registers, 0, sizeof(emu->registers));

  //プログラムカウンタのの初期値を指定されたものにする
  emu->eip = eip;

  //レジスタの初期値を指定されたものにする
  emu->registers[ESP] = esp;

  return emu;
}

//エミュレータを破棄する関数
void destroy_emu(Emulator* emu)
{
  free(emu->memory);
  free(emu);
}

/* 汎用レジスタとプログラムカウンタの値を標準出力に出力する */
void dump_registers(Emulator* emu){
  int i;
  for (int i = 0; i < REGISTERS_COUNT; i++)
  {
    printf("%s = %08x\n", registers_name[i], emu->registers[i]);
  }
  printf("EIP = %08x\n", emu->eip);
}

//メモリ配列の指定した番地から8ビットのデータを取得する関数
//ただし戻り値は32ビットとして表示→これはレジスタサイズが32ビットであるため
uint32_t get_code8(Emulator* emu, int index){
  return emu->memory[emu->eip + index];
}

//メモリ配列の指定した番地から8ビットの符号付き整数のデータを取得する関数
//ただし戻り値は32ビットとして表示→これはレジスタサイズが32ビットであるため
int32_t get_sign_code8(Emulator* emu, int index){
  return (int8_t)emu->memory[emu->eip + index];
}

//メモリ配列の指定した番地から32ビットのデータを取得する関数。
//ただし戻り値は32ビットとして表示→これはレジスタサイズが32ビットであるため
uint32_t get_code32(Emulator* emu, int index) {
  int i;
  uint32_t ret = 0;

  //リトルエンディアンでメモリの値を取得する
  for (int i = 0; i < 4; i++)
  {
    ret |= get_code8(emu, index + i) << (i * 8);
  }

  return ret;
}

//汎用レジスタに32ビットの即値をコピーする関数
void mov_r32_imm32(Emulator* emu) {
  uint8_t reg = get_code8(emu, 0) - 0xB8;
  uint32_t value = get_code32(emu, 1);
  emu->registers[reg] = value;
  emu->eip += 5;
}

//ショートジャンプ命令を実行する関数
void short_jump(Emulator* emu) {
  int8_t diff = get_sign_code8(emu, 1);
  emu->eip += (diff + 2);
}


//関数ポインタテーブルの作成
typedef void instruction_func_t(Emulator*);
instruction_func_t* instructions[256];


//関数ポインタテーブルの（オペコードの値）番目の要素に、そのオペコード自身を代入していく
void init_instructions(void){
  //関数ポインタテーブルの初期化
  int i;
  memset(instructions, 0,sizeof(instructions));

  //汎用レジスタが8個しかないから、ここは8
  //各汎用レジスタに値を32ビットの値をコピーするオペコードを代入していく
  for (int i = 0; i < 8; i++)
  {
    instructions[0xB8 + i] = mov_r32_imm32;
  }

  //ショートジャンプ命令に相当するオペコードを（そのオペコードの値）番目の要素に代入
  instructions[0xEB] = short_jump;
}




/* 汎用レジスタとプログラムカウンタの値を標準出力に出力する */
int main(int argc, char const *argv[])
{
  FILE* binary;
  Emulator* emu;

  if (argc != 2)
  {
    printf("usage: px86 filename\n");
    return 1;
  }

  //EIPが0、ESPが0x7c00の状態のエミュレータを作る
  emu = create_emu(MEMORY_SIZE, 0x0000, 0x7c00);

  //fILE構造体へのポインタ生成
  binary = fopen(argv[1], "rb");

  if (binary == NULL)
  {
    printf("%sファイルが開けません\n", argv[1]);
    return 1;
  }

  //機械語ファイルを読み込む（最大512バイト）
  fread(emu->memory, 1, 0x200, binary);
  fclose(binary);

  //関数ポインタテーブルの作成
  init_instructions();


  //読み込んだ機械語ファイルにしたがって、関数ポインタテーブルから実行オペコードを取り出す
  while (emu->eip < MEMORY_SIZE)
  {
    uint8_t code = get_code8(emu, 0);

    /* 現在のプログラムカウンタと実行されるバイナリを出力する */
    printf("EIP = %X, Code = %02X\n", emu->eip, code);

    if (instructions[code] == NULL)
    {
      //実装されていない命令がきたら終了する
      printf("\nnNot Implemented: %x\n", code);
      break;
    }

    //命令の実行
    instructions[code](emu);

    //EIPが0になったらプログラム終了
    if (emu->eip == 0x00)
    {
      printf("\n\nend of program. \n\n");
      break;
    }
  }

  dump_requires(emu);
  destroy_emu(emu);
  return 0;
}
