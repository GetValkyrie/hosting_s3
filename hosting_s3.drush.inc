<?php

/**
 * @file
 * Drush integration for the S3 service.
 */

/**
 * Implements hook_drush_command().
 */
function hosting_s3_drush_command() {
  $items['s3-delete-bucket'] = array(
    'description' => dt('Delete an S3 bucket.'),
    'arguments' => array(
      'bucket_name' => 'The name of the bucket to delete.',
    ),
    'options' => array(
      'access-key' => 'The AWS IAM access key ID to use when deleting a bucket that is not assocated to a site or backup.',
      'secret-key' => 'The AWS IAM secret access key to use when deleting a bucket that is not assocated to a site or backup.',
      'force' => 'Override checks to ensure buckets in use or tied to a backup are not accidentally deleted.',
    ),
    'allow-additional-options' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_FULL,
  );

  return $items;
}

/**
 * Command callback for the 'drush s3_delete_bucket' command.
 */
function drush_hosting_s3_delete_bucket($bucket_name) {
  // Check if this bucket is associated to a backup
  $sql = "SELECT bid FROM {hosting_s3_backups} WHERE bucket=:bucket_name";
  $result = db_query($sql, array(':bucket_name' => $bucket_name))->fetch();
  if (isset($result->bid)) {
    if (!drush_get_option('force', FALSE)) {
      return drush_set_error('HOSTING_S3_BUCKET_IN_USE', dt('Bucket is associated with a backup. Use \'--force\' to delete the bucket anyway.'));
    }
    else {
      $backup = hosting_site_get_backup($result->bid);
      // NOTE: If the site associated to a backup was deleted, the returned
      // site alias will be '@'.
      $site_alias = hosting_context_name($backup['site']);
    }
  }
  // Check if this bucket is used on a site
  $sql = "SELECT nid FROM {hosting_s3_sites} WHERE s3_bucket_name=:bucket_name";
  $result = db_query($sql, array(':bucket_name' => $bucket_name))->fetch();
  if (isset($result->nid)) {  
    if (!drush_get_option('force', FALSE)) {
      return drush_set_error('HOSTING_S3_BUCKET_IN_USE', dt('Bucket is associated with a site. Use \'--force\' to delete the bucket anyway.'));
    }
    else {
      $site_node = node_load($result->nid);
      $site_alias = hosting_context_name($site_node->nid);
    }
  }

  // Check that we're deleting a site bucket, or a backup bucket associated to
  // a site that still exists. If the site was deleted, or this bucket was
  // never associated to a site or backup, then we cannot load the site alias
  // to retrieve the credentials. So, we prompt for them below.
  if (isset($site_alias) && $site_alias != '@') {
    // Switch to site context.
    d($site_alias, TRUE);
    // Delete the bucket, credentials will be loaded from the associated site.
    d()->service('s3')->deleteBucket($bucket_name);
    if (isset($site_node)) {
      // Delete the associated entry in the hosting_s3_sites db table.
      hosting_s3_node_delete($site_node);
    }
    if (isset($backup)) {
      // Delete the associated entry in the hosting_s3_backups db table.
      hosting_s3_delete_backup($backup['bid']);
      // TODO: Should we delete the backup entirely? It won't be able to be restored properly...
    }
  }
  else {
    // Gather the credentials from command-line options.
    $access_key_id = drush_get_option('access-key', FALSE);
    $secret_access_key = drush_get_option('secret-key', FALSE);
    if ($access_key_id && $secret_access_key) {
      $client = aegir_s3_client_factory($access_key_id, $secret_access_key);
      Provision_Service_s3::deleteBucket($bucket_name, $client);
    }
    else {
      drush_set_error('HOSTING_S3_MISSING_CREDENTIALS', dt('When the bucket to delete is not associated with a site, both \'--access-key\' and \'--secret-key\' options must be provided.'));
    }
  }

}

/**
 * Implements hook_hosting_site_context_options().
 */
function hosting_s3_hosting_site_context_options(&$task) {
  $fields = array('s3_access_key_id', 's3_secret_access_key');
  foreach ($fields as $field) {
    if (isset($task->ref->$field) && !empty($task->ref->$field)) {
      $task->context_options[$field] = $task->ref->$field;
    }
    else {
      $task->context_options[$field] = 'null';
    }
  }
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_s3_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    $fields = array('s3_access_key_id', 's3_secret_access_key', 's3_bucket_name');
    foreach ($fields as $field) {
      if (isset($context->$field) && !empty($context->$field)) {
        $node->$field = $context->$field;
      }
    }
  }
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_s3_post_hosting_install_task($task, $data) {
  if ($task->ref->type == 'site' && isset($data['context']['s3_bucket_name'])) {
    // Save the generated bucket name in the site node.
    // See: Provision_Service_s3::validate_bucket_name().
    drush_log('Saving bucket name in the front end.');
    $task->ref->s3_bucket_name = $data['context']['s3_bucket_name'];
  }
}

/**
 * Implements drush_HOOK_post_hosting_task().
 *
 * We do not use the hook_post_hosting_TYPE_task() hook because it runs too
 * early, we need to run after the backup is run and returns the backup bid to
 * the frontend. So this (clunky) implementation is similar to
 * drush_hosting_post_hosting_task() in hosting/task.hosting.inc.
 *
 * See: https://github.com/drush-ops/drush/pull/462
 */
function drush_hosting_s3_post_hosting_task($tid) {
  $task = &drush_get_context('HOSTING_TASK');
  $data = drush_get_context('HOSTING_DRUSH_OUTPUT');
  $backup_generating_tasks = array('backup', 'restore', 'clone', 'migrate', 'delete');
  # TODO: Add a hook to allow other tasks to declare that they generate a backup that should be registered.
  if (in_array($task->task_type, $backup_generating_tasks) && isset($data['context']['s3_backup_name'])) {
    // Save the backup bucket name associated with the backup ID.
    // See: Provision_Service_s3::pre_backup().
    $bid = hosting_s3_get_latest_site_backup($task->ref->nid);
    $bucket = $data['context']['s3_backup_name'];
    hosting_s3_save_backup($bid, $bucket);
  }
}

/**
 * Implements drush_HOOK_pre_hosting_task()
 */
function drush_hosting_s3_pre_hosting_task() {
  $task =& drush_get_context('HOSTING_TASK');
  if ($task->task_type == 'backup-delete') {
    $bids = array_keys($task->task_args);
    $buckets = array();
    foreach ($bids as $bid) {
      $buckets[] = hosting_s3_load_backup($bid);
    }
    // Pass this to the backend.
    // See: Provision_Service_s3::post_backup_delete().
    $task->options['s3_backups_to_delete'] = $buckets;
    drush_log(dt('Set buckets to delete to: %bucket', array('%bucket' => implode(',', $buckets))));
  }
  if ($task->task_type == 'restore') {
    $s3_restore_bucket = hosting_s3_load_backup($task->task_args['bid']);
    $task->options['s3_restore_bucket'] = $s3_restore_bucket;
    drush_log(dt('Set bucket to restore to: %bucket', array('%bucket' => $s3_restore_bucket)));
  }
}

/**
 * Implements drush_HOOK_post_hosting_TASK_TYPE_task().
 */
function drush_hosting_s3_post_hosting_backup_delete_task($task, $data) {
  $bids = array_keys($task->task_args);
  foreach ($bids as $bid) {
    hosting_s3_delete_backup($bid);
  }
}
