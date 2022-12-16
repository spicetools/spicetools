#include <winsock2.h>
#include <ws2tcpip.h>
#include "netutils.h"
#include <iphlpapi.h>
#include <windows.h>
#include "util/utils.h"


namespace netutils {

    std::vector<std::string> get_local_addresses() {
        std::vector<std::string> return_addresses;

        // use 16KB buffer and resize if needed
        DWORD buffer_size = 16 * 1024;
        IP_ADAPTER_ADDRESSES* adapter_addresses = nullptr;
        for (size_t attempt_no = 0; attempt_no < 3; attempt_no++) {

            // get adapter addresses
            adapter_addresses = (IP_ADAPTER_ADDRESSES*) malloc(buffer_size);
            DWORD error;
            if ((error = GetAdaptersAddresses(
                        AF_UNSPEC,
                        GAA_FLAG_SKIP_ANYCAST |
                        GAA_FLAG_SKIP_MULTICAST |
                        GAA_FLAG_SKIP_DNS_SERVER |
                        GAA_FLAG_SKIP_FRIENDLY_NAME,
                        NULL,
                        adapter_addresses,
                        &buffer_size)) == ERROR_SUCCESS) {

                // success
                break;

            } else if (error == ERROR_BUFFER_OVERFLOW) {

                // retry using new size
                free(adapter_addresses);
                adapter_addresses = NULL;
                continue;

            } else {

                // error - return empty list
                free(adapter_addresses);
                return return_addresses;
            }
        }

        // now iterate the adapters
        for (IP_ADAPTER_ADDRESSES* adapter = adapter_addresses; NULL != adapter; adapter = adapter->Next) {

            // check if loopback
            if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType) {
                continue;
            }

            // iterate all IPv4 and IPv6 addresses
            for (IP_ADAPTER_UNICAST_ADDRESS* adr = adapter->FirstUnicastAddress; adr; adr = adr->Next) {
                switch (adr->Address.lpSockaddr->sa_family) {
                    case AF_INET: {

                        // cast address
                        SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(adr->Address.lpSockaddr);

                        // convert to string
                        char str_buffer[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, &(ipv4->sin_addr), str_buffer, INET_ADDRSTRLEN);

                        // save result
                        return_addresses.push_back(str_buffer);
                        break;
                    }
                    case AF_INET6: {

                        // cast address
                        SOCKADDR_IN6* ipv6 = reinterpret_cast<SOCKADDR_IN6*>(adr->Address.lpSockaddr);

                        // convert to string
                        char str_buffer[INET6_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET6, &(ipv6->sin6_addr), str_buffer, INET6_ADDRSTRLEN);
                        std::string ipv6_str(str_buffer);

                        // skip non-external addresses
                        if (!ipv6_str.find("fe")) {
                            char c = ipv6_str[2];
                            if (c == '8' || c == '9' || c == 'a' || c == 'b') {

                                // link local address
                                continue;
                            }
                        }
                        else if (!ipv6_str.find("2001:0:")) {

                            // special use address
                            continue;
                        }

                        // save result
                        return_addresses.push_back(ipv6_str);
                        break;
                    }
                    default: {
                        continue;
                    }
                }
            }
        }

        // success
        free(adapter_addresses);
        return return_addresses;
    }
}
