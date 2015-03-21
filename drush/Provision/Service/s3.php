<?php
require_once dirname(__FILE__) . '/../../../includes/s3_common.inc';

/**
 * The s3 service class.
 */
class Provision_Service_s3 extends Provision_Service {
  public $service = 's3';

  /**
   * Add the needed properties to the site context.
   */
  static function subscribe_site($context) {
    $context->setProperty('s3_access_key_id');
    $context->setProperty('s3_secret_access_key');
    $context->setProperty('s3_bucket_name');
  }

  /**
   * Wrapper around hook_provision_drupal_create_directories_alter().
   */
  function create_directories_alter(&$dirs, $url) {
    // Create sites/$url and ensure permissions are set (remove from $dirs).
    // This helper can be called optionally to ensure that the site directory
    // is created appropriately before applying other modifications.
    $path = 'sites/' . $url;
    if (!is_dir($path)) {
      provision_file()->mkdir($path)
        ->succeed('Created <code>@path</code>')
        ->fail('Could not create <code>@path</code>', 'DRUSH_PERM_ERROR');
    }

    provision_file()->chmod($path, $dirs[$path], false)
      ->succeed('Changed permissions of <code>@path</code> to @perm')
      ->fail('Could not change permissions <code>@path</code> to @perm');
    unset($dirs[$path]);
  }

  /**
   * Wrapper around hook_provision_provision_drupal_chgrp_directories_alter().
   */
  function chgrp_directories_alter(&$chgrp, $url) {
    // Do nothing here. The function is needed because it's always called,
    // but the actual implementation will be in the storage engine.
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_verify().
   */
  function pre_verify() {
  }

  /**
   * Wrapper around drush_HOOK_provision_install_validate().
   */
  function install_validate() {
    if ($this->validate_credentials()) {
      return $this->validate_bucket_name();
    }
    return FALSE;
  }

  /**
   * Ensure provided credentials are complete and valid.
   */
  function validate_credentials() {
    $access_key_id = d()->s3_access_key_id;
    $secret_access_key = d()->s3_secret_access_key;
    if (aegir_s3_credentials_exist($access_key_id, $secret_access_key, 'handle_missing_keys', $this)) {
      $client = aegir_s3_client_factory($access_key_id, $secret_access_key);
      return aegir_s3_validate_credentials($client, 'handle_validation', 'handle_exception', $this);
    }
    else {  // Missing key.
      return FALSE;
    }
  }

  /**
   * Error handler for missing credentials.
   */
  function handle_missing_keys($missing_keys) {
    foreach ($missing_keys as $key => $label) {
      drush_set_error('ERROR_' . strtoupper($key) . '_MISSING', "Both S3 credentials are required. `$label` is blank.");
    }
    return FALSE;
  }

  /**
   * Success handler for valid credentials.
   */
  function handle_validation() {
    drush_log('S3 credentials validated.', 'ok');
    return TRUE;
  }

  /**
   * Error handler for API exceptions.
   */
  function handle_exception($exception) {
    $code = $exception->getExceptionCode();
    $message= $exception->getMessage();
    $error = array();
    $error[] = t('There was an error validating your S3 credentials.');
    $error[] = t('Error code: @code', array('@code' => $code));
    $error[] = t('Error message: @message', array('@message' => $message));
    drush_set_error('ERROR_S3_EXCEPTION', implode('</li><li>', $error));
    return FALSE;
  }

  /**
   * Ensure generated bucket name is valid for S3.
   */
  function validate_bucket_name(){
    $access_key_id = d()->s3_access_key_id;
    $secret_access_key = d()->s3_secret_access_key;
    $client = aegir_s3_client_factory($access_key_id, $secret_access_key);

    if ($bucket_name = $this->suggest_bucket_name($client, d()->uri)) {
      if ($client->isValidBucketName($bucket_name)) {
        // Pass the bucket name to the front-end.
        // See: hosting_s3_post_hosting_install_task().
        drush_set_option('s3_bucket_name', $bucket_name);
        // Save the bucket name to the context.
        d()->s3_bucket_name = $bucket_name;
        d()->write_alias();
        drush_log(dt('S3 bucket name set to %bucket.', array('%bucket' => $bucket_name)), 'ok');
        return TRUE;
      }
      else {
        drush_set_error('ERROR_INVALID_S3_BUCKET_NAME', dt('Suggested bucket name (%bucket) failed S3 validation.', array('%bucket' => $bucket_name)));
      }
    }
  }

  /**
   * Suggest an available, unique bucket name based on a site's URL.
   */
  function suggest_bucket_name($client, $url) {
    $suggest_base = str_replace('.', '-', gethostname() . '-' . $url);

    if (!$client->doesBucketExist($suggest_base)) {
      return $suggest_base;
    }

    for ($i = 0; $i < 100; $i++) {
      $option = $suggest_base . $i;
      if (!$client->doesBucketExist($option)) {
        return $option;
      }
    }

    drush_set_error('ERROR_S3_BUCKET_NAME_SUGGESTIONS_FAILED', dt("Could not find a free bucket name after 100 attempts"));
    return false;
  }

}
