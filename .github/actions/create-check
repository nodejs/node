# This file is automatically added by @npmcli/template-oss. Do not edit.

name: 'Create Check'
inputs:
  name:
    required: true
  token:
    required: true
  sha:
    required: true
  check-name:
    default: ''
outputs:
  check-id:
    value: ${{ steps.create-check.outputs.check_id }}
runs:
  using: "composite"
  steps:
    - name: Get Workflow Job
      uses: actions/github-script@v7
      id: workflow
      env:
        JOB_NAME: "${{ inputs.name }}"
        SHA: "${{ inputs.sha }}"
      with:
        result-encoding: string
        script: |
          const { repo: { owner, repo}, runId, serverUrl } = context
          const { JOB_NAME, SHA } = process.env

          const job = await github.rest.actions.listJobsForWorkflowRun({
            owner,
            repo,
            run_id: runId,
            per_page: 100
          }).then(r => r.data.jobs.find(j => j.name.endsWith(JOB_NAME)))

          return [
            `This check is assosciated with ${serverUrl}/${owner}/${repo}/commit/${SHA}.`,
            'Run logs:',
            job?.html_url || `could not be found for a job ending with: "${JOB_NAME}"`,
          ].join(' ')
    - name: Create Check
      uses: LouisBrunner/checks-action@v1.6.0
      id: create-check
      with:
        token: ${{ inputs.token }}
        sha: ${{ inputs.sha }}
        status: in_progress
        name: ${{ inputs.check-name || inputs.name }}
        output: |
          {"summary":"${{ steps.workflow.outputs.result }}"}
