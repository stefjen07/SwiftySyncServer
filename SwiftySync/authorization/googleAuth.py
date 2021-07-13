from google.oauth2 import id_token
from google.auth.transport import requests
import sys

CLIENT_ID = sys.argv[0]
token = sys.argv[1]

try:
    idinfo = id_token.verify_oauth2_token(token, requests.Request(), CLIENT_ID)
    userid = idinfo['sub']

except ValueError:
    pass