import { check } from 'k6';
import http from 'k6/http';

export const options = {
  stages: [
    { duration: '1m', target: 50 },
    { duration: '3m', target: 200 },
    { duration: '1m', target: 0 },
  ],
};

// тестовые данные
const search_queries = [
  { first: 'Ив',      second: 'Ив'    },
  { first: 'Ал',      second: 'Ал'    },
  { first: 'Сер',     second: 'Сер'   },
];

const host = __ENV.TEST_HOST || 'app:6000'

export default function () {
  const query = search_queries[__VU % search_queries.length];

  // чтение /user/search
  const search_res = http.get(`${host}/user/search?first_name=${query.first}&last_name=${query.second}`);
  check(search_res, {
    'search status 200': (r) => r.status === 200
  });

  const resp_data = search_res.json()
  if (Array.isArray(resp_data)) {
    for (const item of resp_data) {
      if (item.hasOwnProperty('id')) {
        // чтение /user/get/{id}
        const get_res = http.get(`${host}/user/get/${item.id}`);
        check(get_res, {
          'get status 200': (r) => r.status === 200
        });
      }
    }
  } else {
    console.log('Response body is not a JSON array');
  }
}
