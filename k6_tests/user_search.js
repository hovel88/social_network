import http from 'k6/http';
import { check, sleep } from 'k6';
import { Trend, Rate, Counter } from 'k6/metrics';
import { textSummary } from 'https://jslib.k6.io/k6-summary/0.0.1/index.js';

const metric_search_latency  = new Trend('search_latency', true); // true - enable averaging
const metric_search_success  = new Rate('search_success');
const metric_search_errors   = new Counter('search_errors');
const metric_search_duration = new Trend('search_duration');

export let options = {
  stages: [
    { duration: '30s', target: 1 },
    { duration: '1m', target: 10 },
    { duration: '1m', target: 100 },
    { duration: '1m', target: 1000 },
    { duration: '30s', target: 0 },
  ],
  ext: {
    'prometheus-rw': {
      url: __ENV.K6_PROMETHEUS_RW_SERVER_URL
    },
  }
};

// тестовые данные
const search_queries = [
    { first: 'Ив',      second: 'Ив'    },
    { first: 'Ал',      second: 'Ал'    },
    { first: 'Сер',     second: 'Сер'   },
];

export default function ()
{
    const query = search_queries[__VU % search_queries.length];
    const params = {
        tags: {
            // в переменных окружения может быть два варианта
            // - TEST_TYPE=no-index
            // - TEST_TYPE=with-index
            test_type: __ENV.TEST_TYPE || 'unknown'
        }
    };
    const host = __ENV.TEST_HOST || 'app:6000'

    const start = Date.now();
    const res = http.get(`${host}/user/search?first_name=${query.first}&last_name=${query.second}`, params);
    const duration = Date.now() - start

    metric_search_latency.add(duration, { type: params.tags.test_type });
    metric_search_duration.add(res.timings.duration, { type: params.tags.test_type });
    metric_search_success.add(res.status === 200, { type: params.tags.test_type });
    if (res.status !== 200) metric_search_errors.add(1, { type: params.tags.test_type });

    check(res, {
        'status 200': (r) => r.status === 200,
        'response time <500ms': (r) => r.timings.duration < 500
    });

    sleep(0.1);
}

export function handleSummary(data) {
  return {
    'stdout': textSummary(data, { indent: ' ', enableColors: true }),
  };
}
