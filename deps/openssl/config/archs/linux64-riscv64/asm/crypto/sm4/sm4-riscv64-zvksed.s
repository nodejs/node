.text
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
    .word 33972487 # rk[0:3]
    addi a2, a2, 16
    .word 33972615 # rk[4:7]
    addi a2, a2, 16
    .word 33972743 # rk[8:11]
    addi a2, a2, 16
    .word 33972871 # rk[12:15]
    addi a2, a2, 16
    .word 33972999 # rk[16:19]
    addi a2, a2, 16
    .word 33973127 # rk[20:23]
    addi a2, a2, 16
    .word 33973255 # rk[24:27]
    addi a2, a2, 16
    .word 33973383 # rk[28:31]

    # Load input data
    .word 33906823
    .word 1242865879

    # Encrypt with all keys
    .word 2787647735
    .word 2788696311
    .word 2789744887
    .word 2790793463
    .word 2791842039
    .word 2792890615
    .word 2793939191
    .word 2794987767

    # Save the ciphertext (in reverse element order)
    .word 1242865879
    li t0, -4
    addi a1, a1, 12
    .word 173400231

    ret
.size rv64i_zvksed_sm4_encrypt,.-rv64i_zvksed_sm4_encrypt
.p2align 3
.globl rv64i_zvksed_sm4_decrypt
.type rv64i_zvksed_sm4_decrypt,@function
rv64i_zvksed_sm4_decrypt:
    .word 0xc1027057

    # Order of elements was adjusted in set_decrypt_key()
    .word 33973383 # rk[31:28]
    addi a2, a2, 16
    .word 33973255 # rk[27:24]
    addi a2, a2, 16
    .word 33973127 # rk[23:20]
    addi a2, a2, 16
    .word 33972999 # rk[19:16]
    addi a2, a2, 16
    .word 33972871 # rk[15:11]
    addi a2, a2, 16
    .word 33972743 # rk[11:8]
    addi a2, a2, 16
    .word 33972615 # rk[7:4]
    addi a2, a2, 16
    .word 33972487 # rk[3:0]

    # Load input data
    .word 33906823
    .word 1242865879

    # Encrypt with all keys
    .word 2794987767
    .word 2793939191
    .word 2792890615
    .word 2791842039
    .word 2790793463
    .word 2789744887
    .word 2788696311
    .word 2787647735

    # Save the ciphertext (in reverse element order)
    .word 1242865879
    li t0, -4
    addi a1, a1, 12
    .word 173400231

    ret
.size rv64i_zvksed_sm4_decrypt,.-rv64i_zvksed_sm4_decrypt
# Family Key (little-endian 32-bit chunks)
.p2align 3
FK:
    .word 0xA3B1BAC6, 0x56AA3350, 0x677D9197, 0xB27022DC
.size FK,.-FK
