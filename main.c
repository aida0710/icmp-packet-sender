#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#define PING_PKT_SIZE 32
#define RECV_PKT_SIZE 1024
#define PING_TIMEOUT 3000

// ICMPエコー要求パケットの構造体
struct icmp_echo_request {
    BYTE type;
    BYTE code;
    USHORT checksum;
    USHORT id;
    USHORT sequence;
    char data[PING_PKT_SIZE - sizeof(BYTE) - sizeof(BYTE) - sizeof(USHORT) - sizeof(USHORT) - sizeof(USHORT)];
};

// チェックサムの計算関数
USHORT calculate_checksum(USHORT* buffer, int size) {
    unsigned long cksum = 0;
    while (size > 1) {
        // 16ビットごとに加算
        cksum += *buffer++;
        // バッファのサイズを減らす
        size -= sizeof(USHORT);
    }

    // バッファが1バイト以上ある場合は最後の1バイトを加算
    if (size) {
        // バッファが奇数の場合は最後の1バイトを加算
        cksum += *(UCHAR *)buffer;
    }

    // 32ビットの加算結果を16ビットに折り返す
    cksum = (cksum >> 16) + (cksum & 0xffff);
    //printf("cksum=%lx\n", ~cksum);
    // さらに16ビットの加算結果を16ビットに折り返す
    cksum += cksum >> 16;
    //printf("cksum=%lx\n", ~cksum);

    //最後にビット反転
    return ~cksum;
}

// pingを実行する関数
void ping(const char* host) {
    WSADATA wsaData;
    struct sockaddr_in addr;
    char send_pkt[PING_PKT_SIZE];
    char recv_pkt[RECV_PKT_SIZE];

    // Winsockの初期化
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartupに失敗しました: %d\n", WSAGetLastError());
        exit(1);
    }

    // ソケットの作成
    const SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "ソケットの作成に失敗しました: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    // タイムアウトの設定
    DWORD timeout = PING_TIMEOUT;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    // 宛先アドレスの設定
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);

    // ICMPエコー要求パケットの作成
    struct icmp_echo_request* icmp_req = (struct icmp_echo_request *)send_pkt;
    icmp_req->type = 8; // ICMPエコー要求
    icmp_req->code = 0;
    icmp_req->checksum = 0;
    icmp_req->id = (USHORT)GetCurrentProcessId();
    icmp_req->sequence = 0;

    // チェックサムの計算
    icmp_req->checksum = calculate_checksum((USHORT *)icmp_req, sizeof(struct icmp_echo_request));

    // ICMPエコー要求の送信
    DWORD ret = sendto(sock, send_pkt, PING_PKT_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == SOCKET_ERROR) {
        fprintf(stderr, "送信に失敗しました: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        exit(1);
    }

    // ICMPエコー応答の受信
    LARGE_INTEGER start_time, end_time;
    QueryPerformanceCounter(&start_time);

    ret = recvfrom(sock, recv_pkt, RECV_PKT_SIZE, 0, NULL, NULL);
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            printf("要求がタイムアウトしました。\n");
        }
        else {
            fprintf(stderr, "受信に失敗しました: %d\n", WSAGetLastError());
        }
        closesocket(sock);
        WSACleanup();
        exit(1);
    }

    QueryPerformanceCounter(&end_time);

    // 往復時間の計算
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    const double rtt = (double)(end_time.QuadPart - start_time.QuadPart) / freq.QuadPart * 1000.0;

    // 結果の表示
    printf("%sからの応答: 時間=%.3f ms\n", host, rtt);

    closesocket(sock);
    WSACleanup();
}

/**
* メイン関数
*
* @param argc コマンドライン引数の数
* @param argv コマンドライン引数の配列
* @return 終了ステータス（0: 正常終了, 1: 異常終了）
*/
int main(const int argc, char* argv[]) {
    // 引数の数が正しいかどうかを確認
    if (argc != 3) {
        fprintf(stderr, "使用法: %s <ホスト名> <回数>\n", argv[0]);
        exit(1);
    }

    // 回数が数字であるかどうかを確認
    for (int i = 0; argv[2][i] != '\0'; i++) {
        if (!isdigit(argv[2][i])) {
            fprintf(stderr, "エラー: 整数以外の文字を含めないでください。\n");
            exit(1);
        }
    }

    // 回数を文字列から整数に変換
    const int count = atoi(argv[2]);

    // 回数が1以上の正の整数であるかどうかを確認
    if (count <= 0) {
        fprintf(stderr, "エラー: 1以上の数字を入力してください。\n");
        exit(1);
    }

    // 指定された回数だけpingを実行
    for (int i = 0; i < count; i++) {
        ping(argv[1]);
    }

    return 0;
}
