import http from 'k6/http';
import { check, sleep } from 'k6';
import { Trend, Rate, Counter } from 'k6/metrics';
import { textSummary } from 'https://jslib.k6.io/k6-summary/0.0.1/index.js';

const metric_search_latency      = new Trend('search_latency_ms', true); // true - enable averaging
const metric_search_success_rate = new Rate('search_success_rate');
const metric_search_errors       = new Counter('search_errors_total');
const metric_search_duration     = new Trend('search_duration_ms');

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
      url: __ENV.K6_PROMETHEUS_RW_SERVER_URL,
      metrics: [
        'search_latency_ms_avg',
        'search_latency_ms_max',
        'search_success_rate',
        'search_errors_total',
        'search_duration_ms'
      ],
    },
  },
  // thresholds: {
  //   'search_latency_ms': ['p(95)<500'],  // 95% запросов быстрее 500ms
  //   'search_success_rate': ['rate>0.95'],  // >95% успешных запросов
  // }
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
            test_type: __ENV.TEST_TYPE || 'no-index'
        }
    };
    const host = __ENV.TEST_HOST || 'app:6000'

    const start = Date.now();
    const res = http.get(`${host}/user/search?first_name=${query.first}&last_name=${query.second}`, params);
    const duration = Date.now() - start

    metric_search_latency.add(duration, { type: params.tags.test_type });
    metric_search_duration.add(res.timings.duration);
    metric_search_success_rate.add(res.status === 200);
    if (res.status !== 200) metric_search_errors.add(1);

    // check(res, {
    //     'status 200': (r) => r.status === 200,
    //     'response time <500ms': (r) => r.timings.duration < 500
    // });

    sleep(0.1);
}

export function handleSummary(data) {
  return {
    'stdout': textSummary(data, { indent: ' ', enableColors: true }),
  };
}
