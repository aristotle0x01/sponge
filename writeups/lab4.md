Lab 4 Writeup
=============

This lab took me about two weeks of hours to do. I did not attend the lab session.



Implementation Challenges:

1. pitfalls in understanding the many subtleties, e.g.,  the active state
2. lurking bugs of previous labs, that is, their test coverage is not enough
3. unfamiliarity with the c++ language or features
4. poor performance  far less than 0.1Gbit/s, though all tests passed



Program Structure and Design of the TCPConnection:

1. tshark is a valuable debuging tool in diagnosing what is sent, what is not

   > sudo tshark -Pw /tmp/debug.raw -i tun144

2. colordiff in comparing the result bytes

   > colordiff -y <(xxd /tmp/tmp.UcRoSy6Fje) <(xxd /tmp/tmp.LFmcTiLMwr)

3. valgrind is great in pinpointing performance bottle necks

   > valgrind --tool=callgrind ./apps/tcp_benchmark    // callgrind.out.2849
   >
   > qcachegrind callgrind.out.2849
   >
   > [profile c++](https://stackoverflow.com/questions/375913/how-do-i-profile-c-code-running-on-linux)

4. segfault debug

   > file core
   >
   > gdb -q -nh tests/recv_transmit core
   >
   > [ref](https://linuxtut.com/en/375851ff3f39c4df2253/)

5. performance tips

   a) use ring buffer to avoid array shift in class ByteStream & StreamReassembler

   b) use memcpy to copy chunk, instead of while loop byte by byte

   c) how to mark reassemable consecutive bytes in StreamReassembler is very important



Remaining Bugs:
[None]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
