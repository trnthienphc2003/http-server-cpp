import requests

def test_get_request():
    url = "http://localhost:4221/files/banana"
    response = requests.get(url, headers={
        "User-Agent": "foobar/1.2.3"
    },
        verify=False)  # Disable SSL verification for testing
    assert response.status_code == 200, f"Expected status code 200, got {response.status_code}"
    print(response.text)

test_get_request()