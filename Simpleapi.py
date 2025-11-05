from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/', methods=['POST'])
def receive_message():
    try:
        # Get the JSON data from the request
        data = request.get_json()
        if not data or 'message' in data:
            return jsonify({'error': 'Message key not found in the request'}), 400

        # Extract the message
        message = data

        # Display the message
        print(f"Received message: {message}")

        # Return a success response
        return jsonify({'status': 'Message received', 'message': message}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    # Run the application on the host machine's IP and port 8080
    app.run(host='0.0.0.0', port=8080)
