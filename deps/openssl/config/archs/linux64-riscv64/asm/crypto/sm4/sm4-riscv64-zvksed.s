.text
.section .rodata
.align 4
.Lreverse_index:
    .word 3, 2, 1, 0
.text
.p2align 3
.globl rv64i_zvksed_sm4_cbc_encrypt
.type rv64i_zvksed_sm4_cbc_encrypt,@function
rv64i_zvksed_sm4_cbc_encrypt:
    # check whether the length is a multiple of 16 and >= 16
    li t0, 16
    bltu a2, t0, .Lcbc_enc_end
    andi t0, a2, 15
    bnez t0, .Lcbc_enc_end

    .word 3439489111
    # Load 32 round keys
        # Order of elements was adjusted in set_encrypt_key()
    .word 34007047 # rk[0:3]
    addi a3, a3, 16
    .word 34007175 # rk[4:7]
    addi a3, a3, 16
    .word 34007303 # rk[8:11]
    addi a3, a3, 16
    .word 34007431 # rk[12:15]
    addi a3, a3, 16
    .word 34007559 # rk[16:19]
    addi a3, a3, 16
    .word 34007687 # rk[20:23]
    addi a3, a3, 16
    .word 34007815 # rk[24:27]
    addi a3, a3, 16
    .word 34007943 # rk[28:31]


    # Load IV
    .word 34038791

    # Load the reverse index (for IV updates)
    la t0, .Lreverse_index
    .word 33742855
# =====================================================
# If data length ≥ 64 bytes, process 4 blocks in batch:
# 4-block CBC encryption pipeline:
#   1. Load 4 plaintext blocks
#   2. Reverse bytes for SM4 endianness
#   3. Perform XOR operation with IV or previous ciphertext block (CBC chain)
#   4. Encrypt each data block using the enc_blk function
#   5. Adjust the byte order and store the ciphertext block
#   6. Update the initialization vector (IV)
# If data length < 64 bytes, process it block by block using the Lcbc_enc_single function
# =====================================================
.Lcbc_enc_loop:
    li t0, 64
    bltu a2, t0, .Lcbc_enc_single
    # Load input data0-data3
    .word 33906823
    addi a0, a0, 16
    .word 33906951
    addi a0, a0, 16
    .word 33907079
    addi a0, a0, 16
    .word 33907207
    addi a0, a0, 16
    #XOR with IV
    .word 773062871

    .word 1242865879
    # Encrypt with all keys
        .word 2802327799
    .word 2803376375
    .word 2804424951
    .word 2805473527
    .word 2806522103
    .word 2807570679
    .word 2808619255
    .word 2809667831

    .word 1242865879

    #Update IV to ciphertext block 0
    .word 839910487

    .word 774111575

    .word 1243914583
        .word 2802327927
    .word 2803376503
    .word 2804425079
    .word 2805473655
    .word 2806522231
    .word 2807570807
    .word 2808619383
    .word 2809667959

    .word 1243914583

    #Update IV to ciphertext block 1
    .word 840959063

    .word 775160279

    .word 1244963287
        .word 2802328055
    .word 2803376631
    .word 2804425207
    .word 2805473783
    .word 2806522359
    .word 2807570935
    .word 2808619511
    .word 2809668087

    .word 1244963287

    #Update IV to ciphertext block 2
    .word 842007639

    .word 776208983

    .word 1246011991
        .word 2802328183
    .word 2803376759
    .word 2804425335
    .word 2805473911
    .word 2806522487
    .word 2807571063
    .word 2808619639
    .word 2809668215

    .word 1246011991

    #Update IV to ciphertext block 3
    .word 843056215

    # Save the ciphertext (in reverse element order)
    li t1, -4
            addi t2, a1, 12
        .word 174317735

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174317863

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174317991

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174318119

    addi a1, a1, 16

    addi a2, a2, -64
    bnez a2, .Lcbc_enc_loop
    #Save the final IV
    .word 34038823
    ret

.Lcbc_enc_single:
    # Load input data0
    .word 33906823
    addi a0, a0, 16
    #XOR with IV
    .word 773062871

    .word 1242865879
    # Encrypt with all keys
        .word 2802327799
    .word 2803376375
    .word 2804424951
    .word 2805473527
    .word 2806522103
    .word 2807570679
    .word 2808619255
    .word 2809667831

    .word 1242865879

    # Update IV to ciphertext block 0
    .word 839910487

    # Save the ciphertext (in reverse element order)
    li t1, -4
            addi t2, a1, 12
        .word 174317735

    addi a1, a1, 16
    addi a2, a2, -16

    li t0, 16
    bgeu a2, t0, .Lcbc_enc_single
    # Save the final IV
    .word 34038823
.Lcbc_enc_end:
    ret
.size rv64i_zvksed_sm4_cbc_encrypt,.-rv64i_zvksed_sm4_cbc_encrypt
.p2align 3
.globl rv64i_zvksed_sm4_cbc_decrypt
.type rv64i_zvksed_sm4_cbc_decrypt,@function
rv64i_zvksed_sm4_cbc_decrypt:
    # check whether the length is a multiple of 16 and >= 16
    li t0, 16
    bltu a2, t0, .Lcbc_dec_end
    andi t0, a2, 15
    bnez t0, .Lcbc_dec_end

    .word 3439489111
    # Load IV (in reverse element order)
    li t1, -4
            addi t2, a4, 12
        .word 174318599


    # Load 32 round keys
        # Order of elements was adjusted in set_decrypt_key()
    .word 34007943 # rk[31:28]
    addi a3, a3, 16
    .word 34007815 # rk[27:24]
    addi a3, a3, 16
    .word 34007687 # rk[23:20]
    addi a3, a3, 16
    .word 34007559 # rk[19:16]
    addi a3, a3, 16
    .word 34007431 # rk[15:12]
    addi a3, a3, 16
    .word 34007303 # rk[11:8]
    addi a3, a3, 16
    .word 34007175 # rk[7:4]
    addi a3, a3, 16
    .word 34007047 # rk[3:0]

# =====================================================
# If data length ≥ 128 bytes, process 8 blocks in batch:
# 8-block CBC decryption pipeline:
#   1. Load 8 ciphertext blocks
#   2. Reverse bytes for SM4 endianness
#   3. Use two calls to dec_4blks for decrypting each data block
#   4. XOR with previous ciphertext block (CBC chain)
#   5. Update IV and store plaintext with byte reversal
# =====================================================
.Lcbc_dec_loop:
    li t0, 128
    bltu a2, t0, .Lcbc_check_64
    # Load input data0-data7
    .word 33906823
    addi a0, a0, 16
    .word 33906951
    addi a0, a0, 16
    .word 33907079
    addi a0, a0, 16
    .word 33907207
    addi a0, a0, 16
    .word 33907335
    addi a0, a0, 16
    .word 33907463
    addi a0, a0, 16
    .word 33907591
    addi a0, a0, 16
    .word 33909767
    addi a0, a0, 16

    .word 1242865879
    .word 1243914583
    .word 1244963287
    .word 1246011991
    .word 1247060695
    .word 1248109399
    .word 1249158103
    .word 1266986071
    # Decrypt 8 data blocks
        .word 2809667831
    .word 2809667959
    .word 2809668087
    .word 2809668215

    .word 2808619255
    .word 2808619383
    .word 2808619511
    .word 2808619639

    .word 2807570679
    .word 2807570807
    .word 2807570935
    .word 2807571063

    .word 2806522103
    .word 2806522231
    .word 2806522359
    .word 2806522487

    .word 2805473527
    .word 2805473655
    .word 2805473783
    .word 2805473911

    .word 2804424951
    .word 2804425079
    .word 2804425207
    .word 2804425335

    .word 2803376375
    .word 2803376503
    .word 2803376631
    .word 2803376759

    .word 2802327799
    .word 2802327927
    .word 2802328055
    .word 2802328183

        .word 2809668343
    .word 2809668471
    .word 2809668599
    .word 2809670775

    .word 2808619767
    .word 2808619895
    .word 2808620023
    .word 2808622199

    .word 2807571191
    .word 2807571319
    .word 2807571447
    .word 2807573623

    .word 2806522615
    .word 2806522743
    .word 2806522871
    .word 2806525047

    .word 2805474039
    .word 2805474167
    .word 2805474295
    .word 2805476471

    .word 2804425463
    .word 2804425591
    .word 2804425719
    .word 2804427895

    .word 2803376887
    .word 2803377015
    .word 2803377143
    .word 2803379319

    .word 2802328311
    .word 2802328439
    .word 2802328567
    .word 2802330743

    .word 1242865879
    .word 1243914583
    .word 1244963287
    .word 1246011991
    .word 1247060695
    .word 1248109399
    .word 1249158103
    .word 1266986071

    .word 773062871

    # Update ciphertext to IV (in reverse element order)
    addi t2, a0, -128
            addi t2, t2, 12
        .word 174318599


    .word 774111575

    addi t2, a0, -112
            addi t2, t2, 12
        .word 174318599


    .word 775160279

    addi t2, a0, -96
            addi t2, t2, 12
        .word 174318599


    .word 776208983

    addi t2, a0, -80
            addi t2, t2, 12
        .word 174318599


    .word 777257687

    addi t2, a0, -64
            addi t2, t2, 12
        .word 174318599


    .word 778306391

    addi t2, a0, -48
            addi t2, t2, 12
        .word 174318599


    .word 779355095

    addi t2, a0, -32
            addi t2, t2, 12
        .word 174318599


    .word 797183063

    addi t2, a0, -16
            addi t2, t2, 12
        .word 174318599


    # Save the plaintext (in reverse element order)
            addi t2, a1, 12
        .word 174317735

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174317863

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174317991

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174318119

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174318247

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174318375

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174318503

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174320679

    addi a1, a1, 16

    addi a2, a2, -128
    bnez a2, .Lcbc_dec_loop
    #Save the final IV (in reverse element order)
            addi t2, a4, 12
        .word 174318631

    ret
# =====================================================
# If data length ≥ 64 bytes, process in batches of 4 blocks:
# 4-block CBC decryption process:
#   1. Load 4 ciphertext blocks
#   2. Reverse byte order to fit SM4 byte order
#   3. Decrypt each data block using the dec_4blks function
#   4. XOR with previous ciphertext block (CBC chain)
#   5. Update IV and store plaintext with byte reversal
# If the data length is less than 64 bytes, process it block by block using the Lcbc_dec_single function
# =====================================================
.Lcbc_check_64:
    li t0, 64
    bltu a2, t0, .Lcbc_dec_single
    # Load input data0-data3
    .word 33906823
    addi a0, a0, 16
    .word 33906951
    addi a0, a0, 16
    .word 33907079
    addi a0, a0, 16
    .word 33907207
    addi a0, a0, 16

    .word 1242865879
    .word 1243914583
    .word 1244963287
    .word 1246011991
    # Decrypt 4 data blocks
        .word 2809667831
    .word 2809667959
    .word 2809668087
    .word 2809668215

    .word 2808619255
    .word 2808619383
    .word 2808619511
    .word 2808619639

    .word 2807570679
    .word 2807570807
    .word 2807570935
    .word 2807571063

    .word 2806522103
    .word 2806522231
    .word 2806522359
    .word 2806522487

    .word 2805473527
    .word 2805473655
    .word 2805473783
    .word 2805473911

    .word 2804424951
    .word 2804425079
    .word 2804425207
    .word 2804425335

    .word 2803376375
    .word 2803376503
    .word 2803376631
    .word 2803376759

    .word 2802327799
    .word 2802327927
    .word 2802328055
    .word 2802328183

    .word 1242865879
    .word 1243914583
    .word 1244963287
    .word 1246011991

    .word 773062871

    # Update ciphertext to IV (in reverse element order)
    addi t2, a0, -64
            addi t2, t2, 12
        .word 174318599


    .word 774111575

    addi t2, a0, -48
            addi t2, t2, 12
        .word 174318599


    .word 775160279

    addi t2, a0, -32
            addi t2, t2, 12
        .word 174318599


    .word 776208983

    addi t2, a0, -16
            addi t2, t2, 12
        .word 174318599


    # Save the plaintext (in reverse element order)
            addi t2, a1, 12
        .word 174317735

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174317863

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174317991

    addi a1, a1, 16
            addi t2, a1, 12
        .word 174318119

    addi a1, a1, 16

    addi a2, a2, -64
    bnez a2, .Lcbc_check_64
    #Save the final IV (in reverse element order)
            addi t2, a4, 12
        .word 174318631

    ret

.Lcbc_dec_single:
    # Load input data0
    .word 33906823
    addi a0, a0, 16

    .word 1242865879
    # Decrypt with all keys
        .word 2809667831
    .word 2808619255
    .word 2807570679
    .word 2806522103
    .word 2805473527
    .word 2804424951
    .word 2803376375
    .word 2802327799

    .word 1242865879

    #XOR with IV
    .word 773062871

    # Update ciphertext to IV (in reverse element order)
    li t1, -4
    addi t2, a0, -16
            addi t2, t2, 12
        .word 174318599

    # Save the plaintext (in reverse element order)
            addi t2, a1, 12
        .word 174317735

    addi a1, a1, 16
    addi a2, a2, -16

    li t0, 16
    bgeu a2, t0, .Lcbc_dec_single
    #Save the final IV (in reverse element order)
            addi t2, a4, 12
        .word 174318631

.Lcbc_dec_end:
    ret
.size rv64i_zvksed_sm4_cbc_decrypt,.-rv64i_zvksed_sm4_cbc_decrypt
.p2align 3
.globl rv64i_zvksed_sm4_set_encrypt_key
.type rv64i_zvksed_sm4_set_encrypt_key,@function
rv64i_zvksed_sm4_set_encrypt_key:
    .word 0xc1027057

    # Load the user key
    .word 33906823
    .word 1242865879

    # Load the FK.
    la t0, FK
    .word 33743111

    # Generate round keys.
    .word 772866263
    .word 2249204215 # rk[0:3]
    .word 2251334263 # rk[4:7]
    .word 2252415735 # rk[8:11]
    .word 2253497207 # rk[12:15]
    .word 2254578679 # rk[16:19]
    .word 2255660151 # rk[20:23]
    .word 2256741623 # rk[24:27]
    .word 2257823095 # rk[28:31]

    # Store round keys
    .word 33939879 # rk[0:3]
    addi a1, a1, 16
    .word 33940007 # rk[4:7]
    addi a1, a1, 16
    .word 33940135 # rk[8:11]
    addi a1, a1, 16
    .word 33940263 # rk[12:15]
    addi a1, a1, 16
    .word 33940391 # rk[16:19]
    addi a1, a1, 16
    .word 33940519 # rk[20:23]
    addi a1, a1, 16
    .word 33940647 # rk[24:27]
    addi a1, a1, 16
    .word 33940775 # rk[28:31]

    li a0, 1
    ret
.size rv64i_zvksed_sm4_set_encrypt_key,.-rv64i_zvksed_sm4_set_encrypt_key
.p2align 3
.globl rv64i_zvksed_sm4_set_decrypt_key
.type rv64i_zvksed_sm4_set_decrypt_key,@function
rv64i_zvksed_sm4_set_decrypt_key:
    .word 0xc1027057

    # Load the user key
    .word 33906823
    .word 1242865879

    # Load the FK.
    la t0, FK
    .word 33743111

    # Generate round keys.
    .word 772866263
    .word 2249204215 # rk[0:3]
    .word 2251334263 # rk[4:7]
    .word 2252415735 # rk[8:11]
    .word 2253497207 # rk[12:15]
    .word 2254578679 # rk[16:19]
    .word 2255660151 # rk[20:23]
    .word 2256741623 # rk[24:27]
    .word 2257823095 # rk[28:31]

    # Store round keys in reverse order
    addi a1, a1, 12
    li t1, -4
    .word 174449959 # rk[31:28]
    addi a1, a1, 16
    .word 174449831 # rk[27:24]
    addi a1, a1, 16
    .word 174449703 # rk[23:20]
    addi a1, a1, 16
    .word 174449575 # rk[19:16]
    addi a1, a1, 16
    .word 174449447 # rk[15:12]
    addi a1, a1, 16
    .word 174449319 # rk[11:8]
    addi a1, a1, 16
    .word 174449191 # rk[7:4]
    addi a1, a1, 16
    .word 174449063 # rk[3:0]

    li a0, 1
    ret
.size rv64i_zvksed_sm4_set_decrypt_key,.-rv64i_zvksed_sm4_set_decrypt_key
.p2align 3
.globl rv64i_zvksed_sm4_encrypt
.type rv64i_zvksed_sm4_encrypt,@function
rv64i_zvksed_sm4_encrypt:
    .word 0xc1027057

        # Order of elements was adjusted in set_encrypt_key()
    .word 33974279 # rk[0:3]
    addi a2, a2, 16
    .word 33974407 # rk[4:7]
    addi a2, a2, 16
    .word 33974535 # rk[8:11]
    addi a2, a2, 16
    .word 33974663 # rk[12:15]
    addi a2, a2, 16
    .word 33974791 # rk[16:19]
    addi a2, a2, 16
    .word 33974919 # rk[20:23]
    addi a2, a2, 16
    .word 33975047 # rk[24:27]
    addi a2, a2, 16
    .word 33975175 # rk[28:31]


    # Load input data
    .word 33906823
    .word 1242865879

    # Encrypt with all keys
        .word 2802327799
    .word 2803376375
    .word 2804424951
    .word 2805473527
    .word 2806522103
    .word 2807570679
    .word 2808619255
    .word 2809667831


    # Save the ciphertext (in reverse element order)
    .word 1242865879
    li t1, -4
            addi t2, a1, 12
        .word 174317735


    ret
.size rv64i_zvksed_sm4_encrypt,.-rv64i_zvksed_sm4_encrypt
.p2align 3
.globl rv64i_zvksed_sm4_decrypt
.type rv64i_zvksed_sm4_decrypt,@function
rv64i_zvksed_sm4_decrypt:
    .word 0xc1027057

        # Order of elements was adjusted in set_decrypt_key()
    .word 33975175 # rk[31:28]
    addi a2, a2, 16
    .word 33975047 # rk[27:24]
    addi a2, a2, 16
    .word 33974919 # rk[23:20]
    addi a2, a2, 16
    .word 33974791 # rk[19:16]
    addi a2, a2, 16
    .word 33974663 # rk[15:12]
    addi a2, a2, 16
    .word 33974535 # rk[11:8]
    addi a2, a2, 16
    .word 33974407 # rk[7:4]
    addi a2, a2, 16
    .word 33974279 # rk[3:0]


    # Load input data
    .word 33906823
    .word 1242865879

    # Decrypt with all keys
        .word 2809667831
    .word 2808619255
    .word 2807570679
    .word 2806522103
    .word 2805473527
    .word 2804424951
    .word 2803376375
    .word 2802327799


    # Save the plaintext (in reverse element order)
    .word 1242865879
    li t1, -4
            addi t2, a1, 12
        .word 174317735


    ret
.size rv64i_zvksed_sm4_decrypt,.-rv64i_zvksed_sm4_decrypt
# Family Key (little-endian 32-bit chunks)
.p2align 3
FK:
    .word 0xA3B1BAC6, 0x56AA3350, 0x677D9197, 0xB27022DC
.size FK,.-FK
