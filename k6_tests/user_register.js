import { check } from 'k6';
import http from 'k6/http';

export const options = {
  vus: 100,
  duration: '4m',
};

const host = __ENV.TEST_HOST || 'app:6000'

export default function () {
  const payload = JSON.stringify({
    first_name: `User_${__VU}`,
    second_name: `Last_${__ITER}`,
    birthdate: '1990-01-01',
    biography: 'K6 write (/user/register) test',
    city: 'Saratov',
    password: '$2a$12$XH4BS2zgGgpJs4hiu9p17OwxHxoWto21DLHkzo6JQH67U/3wi.LEW'
  });

  const register_res = http.post(`${host}/user/register`, payload, {
    headers: { 'Content-Type': 'application/json' },
    timeout: '600s'
  });
  check(register_res, {
    'search status 200': (r) => r.status === 200
  });
}