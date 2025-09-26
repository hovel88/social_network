import http from 'k6/http';
import { check, group, sleep } from 'k6';
import { textSummary } from 'https://jslib.k6.io/k6-summary/0.0.1/index.js';

const MESSAGES_PER_PAIR = 1; // сообщений на пару

const host = __ENV.TEST_HOST || 'app:6000'
const test_users = [
    { from: "2f8d9273-3fb3-48ef-b98b-310dacf79316", to: "dc3e666d-97cd-46cf-b151-d942022f435e" },
    { from: "dc3e666d-97cd-46cf-b151-d942022f435e", to: "2f8d9273-3fb3-48ef-b98b-310dacf79316" },
    { from: "e5aedd63-6105-4174-a75c-30fdd511fe15", to: "cf093a1f-2e19-41b6-bcb0-db7d4db793ba" },
    { from: "cf093a1f-2e19-41b6-bcb0-db7d4db793ba", to: "e5aedd63-6105-4174-a75c-30fdd511fe15" },
    { from: "f776b179-828d-47d9-937d-66ab8a9de5b2", to: "d6f778e1-c685-4393-8a7b-abef8596e3e5" },
    { from: "d6f778e1-c685-4393-8a7b-abef8596e3e5", to: "f776b179-828d-47d9-937d-66ab8a9de5b2" }
];

export const options = {
    setupTimeout: '120s',
    stages: [
        { duration: '1m',   target: 10 },
        { duration: '3m',   target: 200 },
        { duration: '1m',   target: 10 }
    ],
    thresholds: {
        // 'http_req_failed{type:send}': ['rate<0.98'],
        // 'http_req_failed{type:list}': ['rate<0.98'],
        'http_req_duration{type:send}': ['p(95)<1200'],
        'http_req_duration{type:list}': ['p(95)<2000'],
        'http_req_waiting{type:send}': ['p(95)<1200'],
        'http_req_waiting{type:list}': ['p(95)<2000']
    }
};

export function setup() {
    for (let i = 0; i < test_users.length; i++) {
        const user1 = test_users[i].from;
        const user2 = test_users[i].to;

        // предварительно создаем N сообщений, в том числе прогревая кеш
        for (let j = 0; j < MESSAGES_PER_PAIR; j++) {
            const url1 = `${host}/dialog/${user2}/send`
            const payload1 = JSON.stringify({
                text: `Message ${j} from ${user1} to ${user2}`
            });
            const params1 = {
                timeout: '600s',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${user1}`
                }
            }

            const url2 = `${host}/dialog/${user1}/send`
            const payload2 = JSON.stringify({
                text: `Message ${j} from ${user2} to ${user1}`
            });
            const params2 = {
                timeout: '600s',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${user2}`
                }
            }

            http.batch([
                { method: 'POST', url: url1, body: payload1, params: params1 },
                { method: 'POST', url: url2, body: payload2, params: params2 }
            ]);

            sleep(0.2);
        }
    }

    return { test_users };
}

export default function(data) {
    const user_from = data.test_users[__ITER % test_users.length].from;
    const user_to   = data.test_users[__ITER % test_users.length].to;

    // чередуем запросы на чтение и запись
    if (__ITER % 2 === 0) {
        group('Send message', function() {
            const url = `${host}/dialog/${user_to}/send`
            const payload = JSON.stringify({
                text: `New message at ${Date.now()} from ${user_from} to ${user_to}`
            });
            const params = {
                tags: { type: 'send' },
                timeout: '600s',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${user_from}`
                }
            };

            http.post(url, payload, params);
            // const send_res = http.post(url, payload, params);
            // check(send_res, {
            //     'send status 200': (r) => r.status === 200
            // });
        });
    } else {
        group('List messages', function() {
            const url = `${host}/dialog/${user_to}/list`
            const params = {
                tags: { type: 'list' },
                timeout: '600s',
                headers: {
                    'Authorization': `Bearer ${user_from}`
                }
            };

            http.get(url, params);
            // const list_res = http.get(url, params);
            // check(list_res, {
            //     'list status 200': (r) => r.status === 200
            // });
        });
    }

    sleep(0.2);
}

export function handleSummary(data) {
    return {
        'stdout': textSummary(data, { indent: ' ', enableColors: true }),
    };
}
