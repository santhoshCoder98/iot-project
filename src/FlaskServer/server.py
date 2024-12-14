from flask import Flask, request, jsonify
from flask_cors import CORS
import boto3
import os
from io import BytesIO

app = Flask(__name__)

# Enable CORS for all routes
CORS(app)

# AWS S3 Configuration
AWS_ACCESS_KEY = "AWS_ACCESS_KEY"
AWS_SECRET_KEY = "AWS_SECRET_KEY"
BUCKET_NAME = "signupdetails"

# Initialize S3 Client
s3_client = boto3.client(
    "s3",
    aws_access_key_id=AWS_ACCESS_KEY,
    aws_secret_access_key=AWS_SECRET_KEY
)

# Dictionary to store all fingerprint templates in memory
fingerprint_templates = {}

@app.route('/upload-template', methods=['POST'])
def upload_template():
    try:
        # Parse JSON data
        data = request.get_json()
        if not data:
            return jsonify({"error": "Invalid JSON payload"}), 400

        # Retrieve Fingerprint ID
        fingerprint_id = data.get("FingerprintID")
        if not fingerprint_id:
            return jsonify({"error": "FingerprintID missing in payload"}), 400

        # Retrieve Fingerprint Data
        fingerprint_data = data.get("FingerprintData")
        if not fingerprint_data:
            return jsonify({"error": "FingerprintData missing in payload"}), 400

        # Log data for debugging
        print(f"Fingerprint ID: {fingerprint_id}")
        print(f"Fingerprint Data: {fingerprint_data}")

        # Process and store the fingerprint data (optional: convert back to binary if needed)
        # Save the fingerprint data to S3 (if required)
        s3_key = f"fingerprints/{fingerprint_id}.txt"
        s3_client.put_object(Bucket=BUCKET_NAME, Key=s3_key, Body=fingerprint_data)

        return jsonify({"message": "Fingerprint template uploaded successfully"}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/', methods=['GET'])
def home():
    return "Hello, World!"

@app.route('/load-templates', methods=['POST'])
def load_templates():
    try:
        global fingerprint_templates

        # List all objects in the S3 bucket's fingerprints directory
        response = s3_client.list_objects_v2(Bucket=BUCKET_NAME, Prefix="fingerprints/")
        if 'Contents' not in response:
            return jsonify({"message": "No fingerprint templates found in the bucket."}), 404

        # Load each template into memory
        for obj in response['Contents']:
            key = obj['Key']
            fingerprint_id = os.path.basename(key).split('.')[0]  # Extract ID from filename
            template_data = s3_client.get_object(Bucket=BUCKET_NAME, Key=key)['Body'].read()
            fingerprint_templates[fingerprint_id] = template_data

        return jsonify({"message": "Templates loaded successfully.", "count": len(fingerprint_templates)}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/verify-template', methods=['POST'])
def verify_template():
    try:
        # Get the template data sent by the fingerprint scanner
        scanned_template = request.data
        if not scanned_template:
            return jsonify({"error": "No template data received."}), 400

        # Compare against loaded templates
        for fingerprint_id, stored_template in fingerprint_templates.items():
            if scanned_template == stored_template:
                return jsonify({"message": "Fingerprint verified.", "fingerprint_id": fingerprint_id}), 200

        return jsonify({"message": "No match found."}), 404

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == "__main__":
    # Ensure templates are loaded on startup
    print("Loading templates from S3 bucket...")
    load_response = None
    try:
        load_response = s3_client.list_objects_v2(Bucket=BUCKET_NAME, Prefix="fingerprints/")
        if 'Contents' in load_response:
            for obj in load_response['Contents']:
                key = obj['Key']
                fingerprint_id = os.path.basename(key).split('.')[0]
                template_data = s3_client.get_object(Bucket=BUCKET_NAME, Key=key)['Body'].read()
                fingerprint_templates[fingerprint_id] = template_data
        print(f"Loaded {len(fingerprint_templates)} templates.")
    except Exception as e:
        print(f"Error loading templates: {str(e)}")

    app.run(host="0.0.0.0", port=5000)
