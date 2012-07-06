#include <stdio.h>
#include <stdint.h>
#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

long start_clk,end_clk;

#define cpuid(func,ax,bx,cx,dx)\
           __asm__ __volatile__ ("cpuid":\
           "=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) : "a" (func));

#if !defined (ALIGN16)
# if defined (__GNUC__)
#  define ALIGN16  __attribute__  ( (aligned (16)))
# else
#  define ALIGN16 __declspec (align (16))
# endif
#endif

__inline long get_Clks(void) {
    long tmp;
    __asm__ volatile(
            "rdtsc\n\t\
            mov %%eax,(%0)\n\t\
            mov %%edx,4(%0)"::"rm"(&tmp):"eax","edx");
    return tmp;
}

int Check_CPU_support_AES()
{
    unsigned int a,b,c,d;
    cpuid(1, a,b,c,d);
    return (c & 0x2000000);
}

/*
extern "C" {
    void AES_128_Key_Expansion(const uint8_t *userkey, uint8_t *key_schedule);
    void AES_192_Key_Expansion(const uint8_t *userkey, uint8_t *key_schedule);
    void AES_256_Key_Expansion(const uint8_t *userkey, uint8_t *key_schedule);

    void AES_ECB_decrypt (const uint8_t *in, uint8_t *out, unsigned long 
    length, const uint8_t *KS, int nr);
    void AES_ECB_encrypt (const uint8_t *in, uint8_t *out, unsigned long 
    length, const uint8_t *KS, int nr);
};


inline __m128i AES_128_ASSIST (__m128i temp1, __m128i temp2)
{
    __m128i temp3;
    temp2 = _mm_shuffle_epi32 (temp2 ,0xff);
    temp3 = _mm_slli_si128 (temp1, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp3 = _mm_slli_si128 (temp3, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp3 = _mm_slli_si128 (temp3, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp1 = _mm_xor_si128 (temp1, temp2);
    return temp1;
}


void AES_128_Key_Expansion (const unsigned char *userkey,
                            unsigned char *key)
{
    __m128i temp1, temp2;
    __m128i *Key_Schedule = (__m128i*)key;
    temp1 = _mm_loadu_si128((__m128i*)userkey);
    Key_Schedule[0] = temp1;

    // __builtin_ia32_aeskeygenassist128((temp1), (0x1));

    temp2 = _mm_aeskeygenassist_si128 (temp1 ,0x1);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[1] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x2);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[2] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x4);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[3] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x8);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[4] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x10);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[5] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x20);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[6] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x40);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[7] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x80);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[8] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x1b);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[9] = temp1;
    temp2 = _mm_aeskeygenassist_si128 (temp1,0x36);
    temp1 = AES_128_ASSIST(temp1, temp2);
    Key_Schedule[10] = temp1;
	

}    

inline void KEY_256_ASSIST_1(__m128i* temp1, __m128i * temp2)
{
    __m128i temp4;
    *temp2 = _mm_shuffle_epi32(*temp2, 0xff);
    temp4 = _mm_slli_si128 (*temp1, 0x4);
    *temp1 = _mm_xor_si128 (*temp1, temp4);
    temp4 = _mm_slli_si128 (temp4, 0x4);
    *temp1 = _mm_xor_si128 (*temp1, temp4);
    temp4 = _mm_slli_si128 (temp4, 0x4);
    *temp1 = _mm_xor_si128 (*temp1, temp4);
    *temp1 = _mm_xor_si128 (*temp1, *temp2);
}

inline void KEY_256_ASSIST_2(__m128i* temp1, __m128i * temp3)
{
    __m128i temp2,temp4;temp4 = _mm_aeskeygenassist_si128 (*temp1, 0x0);
    temp2 = _mm_shuffle_epi32(temp4, 0xaa);
    temp4 = _mm_slli_si128 (*temp3, 0x4);
    *temp3 = _mm_xor_si128 (*temp3, temp4);
    temp4 = _mm_slli_si128 (temp4, 0x4);
    *temp3 = _mm_xor_si128 (*temp3, temp4);
    temp4 = _mm_slli_si128 (temp4, 0x4);
    *temp3 = _mm_xor_si128 (*temp3, temp4);
    *temp3 = _mm_xor_si128 (*temp3, temp2);
}

void AES_256_Key_Expansion (const unsigned char *userkey, unsigned char *key)
{
	__m128i temp1, temp2, temp3;
	__m128i *Key_Schedule = (__m128i*)key;
	temp1 = _mm_loadu_si128((__m128i*)userkey);
	temp3 = _mm_loadu_si128((__m128i*)(userkey+16));
	Key_Schedule[0] = temp1;
	Key_Schedule[1] = temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x01);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[2]=temp1;
	KEY_256_ASSIST_2(&temp1, &temp3);
	Key_Schedule[3]=temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x02);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[4]=temp1;
	KEY_256_ASSIST_2(&temp1, &temp3);
	Key_Schedule[5]=temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x04);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[6]=temp1;
	KEY_256_ASSIST_2(&temp1, &temp3);
	Key_Schedule[7]=temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x08);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[8]=temp1;
	KEY_256_ASSIST_2(&temp1, &temp3);
	Key_Schedule[9]=temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x10);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[10]=temp1;
	KEY_256_ASSIST_2(&temp1, &temp3);
	Key_Schedule[11]=temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x20);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[12]=temp1;
	KEY_256_ASSIST_2(&temp1, &temp3);
	Key_Schedule[13]=temp3;
	temp2 = _mm_aeskeygenassist_si128 (temp3,0x40);
	KEY_256_ASSIST_1(&temp1, &temp2);
	Key_Schedule[14]=temp1;
}

void AES_ECB_encrypt(const unsigned char *in,  //pointer to the PLAINTEXT
		     unsigned char *out,  //pointer to the CIPHERTEXT buffer
		     unsigned long length, //text length in bytes
		     const char *key,  //pointer to the expanded key schedule
		     int number_of_rounds) //number of AES rounds 10,12 or 14
{
    __m128i tmp;
    int i,j;
    if(length%16)
	length = length/16+1;
    else
        length = length/16;

    for(i=0; i < length; i++)
    {
        tmp = _mm_loadu_si128 (&((__m128i*)in)[i]);
        tmp = _mm_xor_si128 (tmp,((__m128i*)key)[0]);
        for(j=1; j <number_of_rounds; j++)
        {
            tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[j]);
        }
        tmp = _mm_aesenclast_si128 (tmp,((__m128i*)key)[j]);
        _mm_storeu_si128 (&((__m128i*)out)[i],tmp);
    }
}



// in: pointer to 16 bytes
// out: pointer to 16 bytes
// key: full 10-round keyschedule
void AES_prf(const unsigned char *in, unsigned char *out, const unsigned char *key) 
{
    __m128i tmp;
    tmp = _mm_loadu_si128 (&((__m128i*)in)[0]);
    tmp = _mm_xor_si128 (tmp,((__m128i*)key)[0]);

    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[1]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[2]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[3]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[4]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[5]);

    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[6]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[7]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[8]);
    tmp = _mm_aesenc_si128 (tmp,((__m128i*)key)[9]);

    tmp = _mm_aesenclast_si128 (tmp,((__m128i*)key)[10]);
    _mm_storeu_si128 (&((__m128i*)out)[0],tmp);

}

__m128i aes(const __m128i msg, const unsigned char *key)
{
    __m128i state = _mm_xor_si128 (msg,((__m128i*)key)[0]);

    for (int i = 1; i < 10; i++)
    {
        state = _mm_aesenc_si128 (state,((__m128i*)key)[i]);
    }

    return _mm_aesenclast_si128 (state,((__m128i*)key)[10]);
}

void makeAND(unsigned int wire1, unsigned int wire2, const unsigned char *prfkey)
{
    unsigned int w1[4];
    unsigned int w2[4];

    w1[0]=wire1; w2[0]=wire2;
    
    __m128i tmp1, tmp2;
    tmp1 = _mm_loadu_si128 (&((__m128i*)w1)[0]);
    tmp2 = _mm_loadu_si128 (&((__m128i*)w2)[0]);
    tmp1 = _mm_xor_si128 (tmp1,((__m128i*)prfkey)[0]);
    tmp2 = _mm_xor_si128 (tmp2,((__m128i*)prfkey)[0]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[1]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[1]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[2]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[2]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[3]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[3]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[4]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[4]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[5]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[5]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[6]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[6]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[7]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[7]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[8]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[8]);

    tmp1 = _mm_aesenc_si128 (tmp1,((__m128i*)prfkey)[9]);
    tmp2 = _mm_aesenc_si128 (tmp2,((__m128i*)prfkey)[9]);

    tmp1 = _mm_aesenclast_si128 (tmp1,((__m128i*)prfkey)[10]);
    tmp2 = _mm_aesenclast_si128 (tmp2,((__m128i*)prfkey)[10]);

    _mm_storeu_si128 (&((__m128i*)w1)[0],tmp1);
    _mm_storeu_si128 (&((__m128i*)w2)[0],tmp2);
}



void AES_CBC_encrypt(const unsigned char *in,
                     unsigned char *out,
                     unsigned char ivec[16],
                     unsigned long length,
                     unsigned char *key,
                     int number_of_rounds)
{
    __m128i feedback,data;
    int i,j;
    if (length%16)
        length = length/16+1;
    else length /=16;
    feedback=_mm_loadu_si128 ((__m128i*)ivec);
    for(i=0; i < length; i++){
        data = _mm_loadu_si128 (&((__m128i*)in)[i]);
        feedback = _mm_xor_si128 (data,feedback);
        feedback = _mm_xor_si128 (feedback,((__m128i*)key)[0]);
        for(j=1; j <number_of_rounds; j++)
            feedback = _mm_aesenc_si128 (feedback,((__m128i*)key)[j]);
        feedback = _mm_aesenclast_si128 (feedback,((__m128i*)key)[j]);
        _mm_storeu_si128 (&((__m128i*)out)[i],feedback);
    }
}

void AES_CTR_encrypt (const unsigned char *in,
                      unsigned char *out,
                    const unsigned char ivec[8],
                    const unsigned char nonce[4],
                    unsigned long length,
                    const unsigned char *key,
                    int number_of_rounds)
{
  __m128i ctr_block, tmp, ONE, BSWAP_EPI64;

  int i,j;
  if (length%16)
    length = length/16 + 1;
  else
    length/=16;

  ONE = _mm_set_epi32(0,1,0,0);

  BSWAP_EPI64 = _mm_setr_epi8(7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8);
  ctr_block = _mm_insert_epi64(ctr_block, *(long long*)ivec, 1);
  ctr_block = _mm_insert_epi32(ctr_block, *(long*)nonce, 1);
  ctr_block = _mm_srli_si128(ctr_block, 4);
  ctr_block = _mm_shuffle_epi8(ctr_block, BSWAP_EPI64);
  ctr_block = _mm_add_epi64(ctr_block, ONE);
  for(i=0; i < length; i++){
    tmp = _mm_shuffle_epi8(ctr_block, BSWAP_EPI64);
    ctr_block = _mm_add_epi64(ctr_block, ONE);
    tmp = _mm_xor_si128(tmp, ((__m128i*)key)[0]);
  }
}


//unsigned char *userkey points to the cipher key
//unsigned char *data points to 16 bytes of data to be encrypted
void AES_128_ENCRYPT_on_the_fly (const unsigned char *userkey,
				 const unsigned char *data)
{
    __m128i temp1, temp2, temp3;
    __m128i block;
    __m128i shuffle_mask = _mm_set_epi32(0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d);
    __m128i rcon;
    int i;
    block = _mm_loadu_si128((__m128i*)&data[0]);
    temp1 = _mm_loadu_si128((__m128i*)userkey);
    rcon  = _mm_set_epi32(1,1,1,1);
    block = _mm_xor_si128(block, temp1);
    for (i=1; i<=8; i++){
	temp2 = _mm_shuffle_epi8(temp1, shuffle_mask);
	temp2 = _mm_aesenclast_si128 (temp2,rcon);
	rcon  = _mm_slli_epi32(rcon,1);
	temp3 = _mm_slli_si128 (temp1, 0x4);
	temp1 = _mm_xor_si128 (temp1, temp3);
	temp3 = _mm_slli_si128 (temp3, 0x4);
	temp1 = _mm_xor_si128 (temp1, temp3);
	temp3 = _mm_slli_si128 (temp3, 0x4);
	temp1 = _mm_xor_si128 (temp1, temp3);
	temp1 = _mm_xor_si128 (temp1, temp2);
	block = _mm_aesenc_si128 (block, temp1);
    }
    rcon = _mm_set_epi32(0x1b,0x1b,0x1b,0x1b);
    temp2 = _mm_shuffle_epi8(temp1, shuffle_mask);
    temp2 = _mm_aesenclast_si128 (temp2,rcon);
    rcon = _mm_slli_epi32(rcon,1);
    temp3 = _mm_slli_si128 (temp1, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp3 = _mm_slli_si128 (temp3, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp3 = _mm_slli_si128 (temp3, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp1 = _mm_xor_si128 (temp1, temp2);
    block = _mm_aesenc_si128 (block, temp1);
    temp2 = _mm_shuffle_epi8(temp1, shuffle_mask);
    temp2 = _mm_aesenclast_si128 (temp2,rcon);
    temp3 = _mm_slli_si128 (temp1, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp3 = _mm_slli_si128 (temp3, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp3 = _mm_slli_si128 (temp3, 0x4);
    temp1 = _mm_xor_si128 (temp1, temp3);
    temp1 = _mm_xor_si128 (temp1, temp2);
    block = _mm_aesenclast_si128 (block, temp1);
    _mm_storeu_si128((__m128i*)&data[0] ,block);
}

#include <openssl/sha.h>
#include <vector>

typedef unsigned char byte;
typedef std::vector<byte> Bytes;

Bytes shen_sha256(const Bytes &data, const size_t bits)
{
	static const byte MASK[8] =
		{ 0xFF, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };

    byte buf[SHA_DIGEST_LENGTH];
    SHA_CTX sha256;

    SHA1_Init(&sha256);
    SHA1_Update(&sha256, &data[0], data.size());
    SHA1_Final(buf, &sha256);

	Bytes hash(buf, buf + (bits+7)/8);
	hash.back() &= MASK[bits % 8]; // clear the extra bits

	return hash;
}

#include <openssl/aes.h>
#include <openssl/evp.h>

#include <ctime>
#include <cstring>

static EVP_CIPHER_CTX ctx;

static void AES128_encrypt_opessl(const uint8_t *in, uint8_t *out, const 
uint8_t *key)
{
	int out_len;

	EVP_EncryptInit(&ctx, EVP_aes_128_ecb(), key, 0);
	EVP_EncryptUpdate(&ctx, out, &out_len, in, 16);
}

static void AES256_encrypt_openssl(const uint8_t *in, uint8_t *out, const 
uint8_t *key)
{
	int out_len;

	EVP_EncryptInit(&ctx, EVP_aes_256_ecb(), key, 0);
	EVP_EncryptUpdate(&ctx, out, &out_len, in, 16);
}

#include <wmmintrin.h>

static void AES128_encrypt_aesni(const uint8_t *in, uint8_t *out, const uint8_t 
*key)
{
    ALIGN16 uint8_t KEY[16*11];
    ALIGN16 uint8_t PLAINTEXT[64];
    ALIGN16 uint8_t CIPHERTEXT[64];

    AES_128_Key_Expansion(key, KEY);
    _mm_storeu_si128(&((__m128i*)PLAINTEXT)[0],*(__m128i*)in);
    AES_ECB_encrypt(PLAINTEXT, CIPHERTEXT, 64, KEY, 10);
    _mm_storeu_si128((__m128i*)out,((__m128i*)CIPHERTEXT)[0]);
}

static int AES256_encrypt_aesni(const uint8_t *in, uint8_t *out, const uint8_t 
*key)
{
    ALIGN16 uint8_t KEY[16*15];
    ALIGN16 uint8_t PLAINTEXT[64];
    ALIGN16 uint8_t CIPHERTEXT[64];

    AES_256_Key_Expansion(key, KEY);
    _mm_storeu_si128(&((__m128i*)PLAINTEXT)[0],*(__m128i*)in);
    AES_ECB_encrypt(PLAINTEXT, CIPHERTEXT, 64, KEY, 14);
    _mm_storeu_si128((__m128i*)out,((__m128i*)CIPHERTEXT)[0]);

    return 0;
}

int test_AES()
{
    int c =  Check_CPU_support_AES();
    printf("cpu support: %x\n", c );
    int i;

    unsigned char userKey[] =
    {
        0xf1, 0x34, 0xa4, 0x89,
        0x33, 0x81, 0x1a, 0x6e,
        0x94, 0x12, 0x52, 0xbc,
        0xb5, 0xe5, 0xff, 0xc3
    };
    
    unsigned char key[16*11];
    unsigned char in[16];
    unsigned char out[16];

    //AES_128_Key_Expansion (userKey, key);

    start_clk=get_Clks();
    end_clk=get_Clks();
    long tt = end_clk-start_clk;
    printf("test clicks: %lld\n",tt);
    double per;

    clock_t start_time, end_time;

    __m128i msg = _mm_loadu_si128 (&((__m128i*)userKey)[0]);
    const int TIMES = 5000000;

    start_time = clock();
    start_clk=get_Clks();
    for ( i=0; i<TIMES; i++) {
        //((unsigned int*)(in))[0] = i;
        //AES_128_Key_Expansion (userKey, key);
        //makeAND(i, i+1, key);
        AES128_encrypt_aesni((unsigned char *)&msg, (unsigned char *)&msg, 
        userKey);
        //AES_128_ENCRYPT_on_the_fly(userKey, (unsigned char *)&msg);
        //msg = aes(msg, key);
        //AES_prf(in, out, key);
        //if (i%10000000 == 0) { printf("%4d\n",i); }
    }
    end_clk=get_Clks();
    end_time = clock();

    tt = end_clk-start_clk;
    per = tt/(double)TIMES;
    printf("aesni total clicks:%lld, ave clicks:%f, msg:%d\n",tt,per, ((unsigned int*)&msg)[0]);
    printf("aesni time: %f\n",(end_time-start_time)/(double)CLOCKS_PER_SEC);
    
    Bytes data(userKey, userKey+sizeof(userKey));

    start_time = clock();
    start_clk=get_Clks();
    for ( i=0; i<TIMES; i++) {
        //data = shen_sha256(data, data.size()*8);
        data = shen_sha256(data, data.size()*8);
    }
    end_clk=get_Clks();
    end_time = clock();

    tt = end_clk-start_clk;
    per = tt/(double)TIMES;
    printf("sha256 clicks: total tics:%lld, ave tics:%f, msg:%d\n",tt,per, ((unsigned int*)&data[0])[0]);
    printf("sha256 time: %f\n",(end_time-start_time)/(double)CLOCKS_PER_SEC);

    AES_KEY aes_key;

    Bytes seed = data;
    
    Bytes hashed_seed = shen_sha256(seed, AES_BLOCK_SIZE*8);
    memset(&aes_key, 0, sizeof(AES_KEY));
    AES_set_encrypt_key(&hashed_seed[0], AES_BLOCK_SIZE*8, &aes_key);
    data = shen_sha256(seed, AES_BLOCK_SIZE*8);
    

    Bytes hashed_seed = shen_sha256(seed, AES_BLOCK_SIZE*8);
    memset(&aes_key, 0, sizeof(AES_KEY));
    AES_set_encrypt_key(&hashed_seed[0], AES_BLOCK_SIZE*8, &aes_key);
    data = shen_sha256(seed, AES_BLOCK_SIZE*8);

    start_time = clock();
    start_clk=get_Clks();
    for ( i=0; i<TIMES; i++) {
        long long cnt;
        AES_ecb_encrypt(&data[0], &data[0], &aes_key, AES_ENCRYPT);
    // update state
        //cnt = *(long long*)(&data[data.size()-1-sizeof(cnt)]);
        //cnt++;
        // *(long long*)(&data[data.size()-1-sizeof(cnt)]) = cnt;
        //AES_ecb_encrypt(&data[0], &data[0], &aes_key, AES_ENCRYPT);
    // update state
        //cnt = *(long long*)(&data[data.size()-1-sizeof(cnt)]);
        //cnt++;
        // *(long long*)(&data[data.size()-1-sizeof(cnt)]) = cnt;
    }
    end_clk=get_Clks();
    end_time = clock();

    tt = end_clk-start_clk;
    per = tt/(double)TIMES;
    printf("shen_aes clicks: %lld %f\n%d\n",tt,per, ((unsigned int*)&data[0])[0]);
    printf("shen_aes time: %f\n",(end_time-start_time)/(double)CLOCKS_PER_SEC);

    return 0;
}
*/

#include <stdint.h>
#include <ext/hash_map>

struct my_hash
{
  size_t
  operator()(uint64_t __x) const
  { return static_cast<size_t>(__x); }
};

typedef __gnu_cxx::hash_map<uint64_t, std::pair<uint32_t, uint32_t>, my_hash > MAP;

int main()
{
    long start_clk, end_clk;
    const int TIMES = 10000000;
    MAP map;

    start_clk=get_Clks();

    int c =  Check_CPU_support_AES();
    printf("cpu support: %x\n", c );

    end_clk=get_Clks();
    long tt = end_clk-start_clk;
    printf("test clicks: %lld\n",tt);

    c =  Check_CPU_support_AES();
    printf("cpu support: %x\n", c );

    start_clk=get_Clks();
    
    for (int i = 0; i < TIMES; i++)
    {
        map[static_cast<uint64_t>(2*i+0)] = std::make_pair(i, i);
        map[static_cast<uint64_t>(2*i+1)] = std::make_pair(i, i);
    }
    
    end_clk=get_Clks();
    tt = end_clk-start_clk;
    printf("clicks: %f\n", tt/(double)TIMES/2); 

    return 0;
}
