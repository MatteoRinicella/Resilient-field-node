from flask import Flask, jsonify

GATEWAY_HOST = "0.0.0.0"
GATEWAY_PORT = 8080

app = Flask(__name__)

@app.get("/health")
def health():
    return jsonify(
        {
            "status": "ok",
            "service": "resilient-field-node-gateway"
        }
    )

def main()-> None:
    print(
        f"Gateway Flask in ascolto su "
        f"http://localhost:{GATEWAY_PORT}"
    )

    app.run(
        host=GATEWAY_HOST,
        port=GATEWAY_PORT,
        debug=False,
    )

if __name__ == "__main__":
    main()