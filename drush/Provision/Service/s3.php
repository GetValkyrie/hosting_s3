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
   * Drush hooks.
   *
   * Methods in this section are called from ../../provision_s3.drush.inc
   */

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
   * Wrapper around hook_provision_drupal_config().
   */
  function drupal_config($uri, $data) {
    $creds = $this->get_credentials();
    $bucket = $this->get_bucket_name();

    drush_log('Injecting S3 bucket and credentials into site settings.php');
    $lines = array();
    $lines[] = "  \$conf['aws_key'] = '" . $creds['access_key_id'] . "';";
    $lines[] = "  \$conf['aws_secret'] = '" . $creds['secret_access_key'] . "';";
    $lines[] = "  \$conf['amazons3_bucket'] = '" . $bucket . "';";

    return implode("\n", $lines);
  }

  /**
   * Wrapper around drush_HOOK_provision_install_validate().
   */
  function install_validate() {
    if ($this->validate_credentials()) {
      $bucket_name = $this->suggest_bucket_name();
      if ($this->validate_bucket_name($bucket_name)) {
        $this->save_bucket_name($bucket_name);
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_install().
   */
  function pre_install() {
    $this->create_bucket();
    $this->test_bucket();
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_verify().
   */
  function pre_verify() {
    $this->validate_credentials();
    $this->test_bucket();
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_backup().
   */
  function pre_backup() {
    if ($this->backup_site_bucket()) {
      $this->override_backup_filename();
    }
  }

  /**
   * Wrapper around drush_HOOK_provision_backup_rollback().
   */
  function pre_backup_rollback() {
    if ($bucket = drush_get_option('s3_backup_name', FALSE)) {
      $this->delete_bucket($bucket);
    }
    else {
      drush_log("'s3_backup_name' option not set.",'warning');
    }
  }

  /**
   * Wrapper around drush_HOOK_post_provision_backup().
   */
  function post_backup() {
    $this->inject_backup_settings();
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_restore().
   */
  function pre_restore() {
    if ($restore_bucket = drush_get_option('s3_restore_bucket', FALSE)) {
      $this->restore_site_bucket($restore_bucket);
    }
  }

  /**
   * Wrapper around drush_HOOK_post_provision_restore().
   */
  function post_restore() {
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_backup_delete().
   */
  function pre_backup_delete() {
  }

  /**
   * Wrapper around drush_HOOK_post_provision_backup_delete().
   */
  function post_backup_delete() {
    $backups = drush_get_option('s3_backups_to_delete', array());
    foreach ($backups as $backup) {
      $this->delete_bucket($backup);
    }
  }

  /**
   * Wrapper around drush_HOOK_pre_provision_delete().
   */
  function pre_delete() {
    // TODO: We usually take a site backup prior to deletion.
    // Should we sync the bucket locally for such a backup?
    $this->delete_bucket();
  }


  /**
   * Helper methods.
   */

  /**
   * Back up a site bucket.
   *
   * @return
   *   The generated name of the backup bucket.
   */
  function backup_site_bucket() {
    $site_bucket = $this->get_bucket_name();
    $client = $this->client_factory();
    if ($client->doesBucketExist($site_bucket)) {
      drush_log(dt('Backing up site bucket (%bucket).', array('%bucket' => $site_bucket)));
      $backup_bucket = $this->suggest_bucket_name();
      // Save new bucket name to be used in the settings.php packaged in the
      // backup tarball. See: drupal_config(). This also allows us to pass the
      // bucket name to the front-end in hosting_s3_post_hosting_backup_task(),
      // and delete it in pre_backup_rollback().
      drush_set_option('s3_backup_name', $backup_bucket);

      if ($this->validate_bucket_name($backup_bucket)) {
        return $this->copy_bucket($site_bucket, $backup_bucket);
      }
    }
    else {
      drush_log(dt('Could not backup site bucket (%bucket). Bucket does not exist.',
        array('%bucket' => $site_bucket)), 'warning');
      return FALSE;
    }
  }

  /**
   * Inject backup bucket name into settings.php packaged with backup.
   */
  function inject_backup_settings() {
    $orig_backup_file = drush_get_option('s3_orig_backup_file', FALSE);
    $bucket = drush_get_option('s3_backup_name', FALSE);
    if ($orig_backup_file && $bucket) {
      drush_log('Injecting backup bucket name into settings.php packaged with backup.');
      $backup_file = drush_get_option('backup_file');
      $tmpdir = drush_tempdir();

      drush_log('Extracting settings.php from backup.');
      drush_shell_exec("tar -C '$tmpdir' -x -f '$backup_file' './settings.php'");
      provision_file()->chmod("$tmpdir/settings.php", 0640);

      drush_log('Appending backup bucket to settings.php.');
      # TODO: replace the relevant line instead?
      $lines = "\n";
      $lines .= "  # backup bucket name override\n";
      $lines .= "  \$conf['amazons3_bucket'] = '$bucket';\n";
      file_put_contents("$tmpdir/settings.php", $lines, FILE_APPEND);
      provision_file()->chmod("$tmpdir/settings.php", 0440);

      drush_log('Appending new settings file to the backup tar.');
      # N.B. In pre_backup(), we are ensured that the tarball is not gzipped.
      drush_shell_exec("tar -C '$tmpdir' -r -f '$backup_file' './settings.php'");
      # If we were using gzip to begin with...
      if ($backup_file != $orig_backup_file) {
        drush_log('Gzipping backup file.');
        drush_shell_exec("gzip '$backup_file'");
        drush_log('Restoring original filename.');
        drush_set_option('backup_file', drush_get_option('s3_orig_backup_file'));
      }
    }
    else {
      drush_log(dt('Skipping injection of backup bucket name into settings.php packaged with backup.'), 'warning');
    }
  }

  /**
   * Strip .gz from the backup filename. This will stop the backup process
   * from compressing the backup, thus allowing us to operate on a tarfile
   * directly. This, in turn, allows us to append the backup bucket name to
   * the settings.php that is packaged with the backup. See: post_backup().
   */
  function override_backup_filename() {
    drush_log('Overriding backup filename to block gzipping.');
    $backup_file = drush_get_option('backup_file', NULL);
    drush_set_option('s3_orig_backup_file', $backup_file);
    drush_set_option('backup_file', preg_replace('/\.gz$/', '', $backup_file));
  }

  /**
   * Create a new bucket and sync contents from another bucket.
   */
  function copy_bucket($src_bucket, $dest_bucket) {
    $buckets = array(
      '%src_bucket' => $src_bucket,
      '%dest_bucket' => $dest_bucket,
    );

    $client = $this->client_factory();
    drush_log(dt('Copying site bucket %src_bucket to backup bucket %dest_bucket.', $buckets));
    if (!$client->doesBucketExist($dest_bucket)) {
      $this->create_bucket($dest_bucket);
    }
    else {
      drush_log(dt('S3 bucket `%dest_bucket` already exists. Clearing contents.', $buckets));
      // TODO: figure out a smarter way of clearing stale contents to avoid excess data transfer.
      $client->clearBucket($dest_bucket);
    }

    // See: http://stackoverflow.com/questions/21797528/php-how-to-sync-data-between-s3-buckets-using-php-code-without-using-the-cli
    try {
      $client->registerStreamWrapper();
    } catch (Exception $e) {
      return $this->handle_exception($e, dt('Could not register S3 stream wrapper.'));
    }
    try {
      $client->uploadDirectory("s3://$src_bucket", $dest_bucket);
    } catch (Exception $e) {
      return $this->handle_exception($e, dt('Could not copy contents of %src_bucket to %dest_bucket', $buckets));
    }

    drush_log(dt('Copied contents of %src_bucket to %dest_bucket', $buckets), 'success');
    return TRUE;
  }

  /**
   * Return the bucket name from the site context.
   */
  function get_bucket_name() {
    return d()->s3_bucket_name;
  }

  /**
   * Create an S3 bucket.
   */
  function create_bucket($bucket = NULL) {
    if (is_null($bucket)) {
      $bucket = $this->get_bucket_name();
    }
    $client = $this->client_factory();
    if (!$client->doesBucketExist($bucket)) {
      drush_log(dt('Creating S3 bucket `%bucket`.', array('%bucket' => $bucket)));
      $result = $client->createBucket(array(
        'Bucket' => $bucket,
      ));
      // Wait until the bucket is created.
      $client->waitUntilBucketExists(array('Bucket' => $bucket));
      if ($client->doesBucketExist($bucket)) {
        drush_log(dt('Created S3 bucket `%bucket`.', array('%bucket' => $bucket)), 'success');
        return $result;
      }
      else {
        return drush_set_error('ERROR_S3_BUCKET_NOT_CREATED', dt('Could not create S3 bucket `%bucket`.', array('%bucket' => $bucket)));
      }
    }
    else {
      return drush_set_error('ERROR_S3_BUCKET_ALREADY_EXISTS', dt('S3 bucket `%bucket` already exists.', array('%bucket' => $bucket)));
    }
  }

  /**
   * Ensure we can create and delete objects in bucket.
   */
  function test_bucket() {
    $bucket = $this->get_bucket_name();
    $client = $this->client_factory();

    // Generate unique test filename and content.
    $test_key = uniqid("hosting_s3-test-key-", true);
    $test_content = uniqid("hosting_s3-test-content-", true);

    drush_log(dt('Checking access to `%bucket` bucket by uploading test file (%test_key)', array(
      '%bucket' => $bucket,
      '%test_key' => $test_key,
    )));

    $result = $client->putObject(array(
      'Bucket' => $bucket,
      'Key'    => $test_key,
      'Body'   => $test_content,
    ));
    $result = $client->getObject(array(
      'Bucket' => $bucket,
      'Key'    => $test_key
    ));

    if ($result['Body'] == $test_content) {
      drush_log(dt('Successfully uploaded test file to bucket.'), 'success');
      drush_log(dt('Deleting test file (%test_key) from bucket.', array('%test_key' => $test_key)));
      $result = $client->deleteObject(array(
        'Bucket' => $bucket,
        'Key'    => $test_key,
      ));
      return TRUE;
    }
    else {
      return drush_set_error('ERROR_S3_TEST_FILE_NOT_CREATED', 'Could not create test file in S3 bucket.');
    }
  }

  /**
   * Delete a bucket.
   */
  function delete_bucket($bucket = NULL) {
    if (is_null($bucket)) {
      $bucket = $this->get_bucket_name();
    }
    $client = $this->client_factory();

    if ($client->doesBucketExist($bucket)) {
      drush_log(dt('Deleting bucket `%bucket`.', array('%bucket' => $bucket)));

      drush_log(dt('Clearing bucket contents.'));
      $result = $client->clearBucket($bucket);
      drush_log(dt('Cleared bucket contents.'), 'success');

      $result = $client->deleteBucket(array(
        'Bucket' => $bucket
      ));
      // Wait until the bucket is deleted.
      $client->waitUntilBucketNotExists(array('Bucket' => $bucket,));
      if (!$client->doesBucketExist($bucket)) {
        drush_log(dt('Deleted S3 bucket `%bucket`.', array('%bucket' => $bucket)), 'success');
        return $result;
      }
      else {
        return drush_set_error('ERROR_S3_BUCKET_NOT_DELETED', 'Could not delete S3 bucket.');
      }
    }
    else {
      drush_log(dt('Bucket %bucket does not exist, so it cannot be deleted.', array('%bucket' => $bucket)), 'warning');
    }
  }

  /**
   * Restore a site bucket.
   */
  function restore_site_bucket($restore_bucket) {
    $site_bucket = $this->get_bucket_name();
    $client = $this->client_factory();
    if ($client->doesBucketExist($restore_bucket)) {
      drush_log(dt('Restoring site bucket (%bucket).', array('%bucket' => $restore_bucket)));
      return $this->copy_bucket($restore_bucket, $site_bucket);
    }
    else {
      drush_log(dt('Could not restore bucket (%bucket). Bucket does not exist.',
        array('%bucket' => $restore_bucket)), 'warning');
      return FALSE;
    }
  }

  /**
   * Return an S3Client object.
   */
  function client_factory() {
    static $client = NULL;
    if (is_null($client)) {
      $creds = $this->get_credentials();
      if (aegir_s3_credentials_exist($creds['access_key_id'], $creds['secret_access_key'], array($this, 'handle_missing_keys'))) {
        $client = aegir_s3_client_factory($creds['access_key_id'], $creds['secret_access_key']);
      }
    }
    return $client;
  }

  function get_credentials() {
    return array(
      'access_key_id' => d()->s3_access_key_id,
      'secret_access_key' => d()->s3_secret_access_key,
    );
  }

  /**
   * Ensure provided credentials are complete and valid.
   */
  function validate_credentials() {
    return aegir_s3_validate_credentials($this->client_factory(), array($this, 'handle_validation'), array($this, 'handle_exception'));
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
  function handle_exception($exception, $message = 'There was an error in a request to S3.') {
    $code = $exception->getExceptionCode();
    $message= $exception->getMessage();
    $error = array();
    $error[] = $message;
    $error[] = t('Error code: @code', array('@code' => $code));
    $error[] = t('Error message: @message', array('@message' => $message));
    return drush_set_error('ERROR_S3_EXCEPTION', implode('</li><li>', $error));
  }

  /**
   * Ensure generated bucket name is valid for S3.
   */
  function validate_bucket_name($bucket_name){
    $client = $this->client_factory();

    if ($bucket_name = $this->suggest_bucket_name()) {
      if ($client->isValidBucketName($bucket_name)) {
        return TRUE;
      }
      else {
        drush_set_error('ERROR_INVALID_S3_BUCKET_NAME', dt('Suggested bucket name (%bucket) failed S3 validation.', array('%bucket' => $bucket_name)));
        return FALSE;
      }
    }
  }

  /**
   * Save bucket name to context and pass it back to the front-end.
   */
  function save_bucket_name($bucket_name) {
    // Pass the bucket name to the front-end.
    // See: hosting_s3_post_hosting_install_task().
    drush_set_option('s3_bucket_name', $bucket_name);
    // Save the bucket name to the context.
    d()->s3_bucket_name = $bucket_name;
    d()->write_alias();
    drush_log(dt('S3 bucket name set to %bucket.', array('%bucket' => $bucket_name)), 'ok');
  }

  /**
   * Save S3 credential to context and pass it back to the front-end.
   */
  function save_credentials($access_key_id, $secret_access_key) {
    // Pass the credentials to the front-end.
    // See: hosting_s3_post_hosting_import_task().
    drush_set_option('s3_access_key_id', $access_key_id);
    drush_set_option('s3_secret_access_key', $secret_access_key);
    // Save the bucket name to the context.
    d()->s3_access_key_id = $access_key_id;
    d()->s3_secret_access_key = $secret_access_key;
    d()->write_alias();
    drush_log(dt('Saved S3 credentials to site context.'), 'ok');
  }

  /**
   * Suggest an available, unique bucket name based on a site's URL.
   */
  function suggest_bucket_name() {
    $client = $this->client_factory();
    $suggest_base = str_replace('.', '-', gethostname() . '-' . d()->uri);

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
