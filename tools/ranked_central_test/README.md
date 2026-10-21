# Test of the central account service client (Linux)

`codemp/server/sv_ranked_central_core.cpp` is engine-independent so it can be tested on its own. The test checks
SHA-256 / HMAC against the RFC test vectors and drives the real client against a throwaway copy of the account
service (in-memory store), including a service restart (reconnect) and a wrong secret.

    cd codemp/server && gcc -c cJSON.c -o /tmp/cJSON.o
    g++ -std=c++11 -Wall -Wextra -I. ../../tools/ranked_central_test/central_core_test.cpp sv_ranked_central_core.cpp /tmp/cJSON.o -lm -o /tmp/central_core_test
    /tmp/central_core_test <dir containing accounts_core.js> tools/ranked_central_test/test_service.js

`accounts_core.js` is part of the account service (Jedaii Bot folder).
