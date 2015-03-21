<?php
/**
 * @file
 * S3 functions common to both Aegir's front-end and back-end.
 */

require dirname(__FILE__) . '/../vendor/autoload.php';
use Aws\S3\S3Client;
use Aws\Common\Credentials\Credentials;

/**
 * Instantiate and return an S3 client from a set of AWS credentials.
 */
function aegir_s3_client_factory($access_key_id, $secret_access_key) {
  $credentials = new Credentials($access_key_id, $secret_access_key);

  return S3Client::factory(array('credentials' => $credentials));
}

/**
 * Validate S3 credentials.
 */
function aegir_s3_validate_credentials($client, $success_callback, $error_callback, $object = NULL) {
  try {
    // This only validates that the keys themselves are valid, and not that
    // they grant sufficient access to create/sync/delete buckets.
    $result = $client->listBuckets();
  } catch (Exception $e) {
    if (function_exists($error_callback)) {
      return call_user_func($error_callback, $e);
    }
    elseif (!is_null($object) && method_exists($object, $error_callback)) {
      return call_user_method($error_callback, $object, $e);
    }
    return FALSE;  // Fallback if callback isn't implemented
  }
  if (isset($result)) {
    if (function_exists($success_callback)) {
      return call_user_func($success_callback);
    }
    return TRUE;  // Fallback if callback isn't implemented
  }
  return FALSE;  // Default if something goes wrong. We should never get here.
}

function aegir_s3_credentials_exist($access_key_id, $secret_access_key, $missing_key_callback, $object = NULL) {
  $missing_keys = array();
  if (empty($access_key_id)) {
    $missing_keys['s3_access_key_id'] = 'Access key ID';
  }
  if (empty($secret_access_key)) {
    $missing_keys['s3_secret_access_key'] = 'Secret access key';
  }
  if (count($missing_keys)) {
    if (function_exists($missing_key_callback)) {
      return call_user_func($missing_key_callback, $missing_keys);
    }
    elseif (!is_null($object) && method_exists($object, $missing_key_callback)) {
      return call_user_method($missing_key_callback, $object, $missing_keys);
    }
    return FALSE;  // Fallback if callback isn't implemented
  }
  else {
    return TRUE;
  }
}
